#!/usr/bin/env python3
# Regenerates the streams avchold plays: short 480x272 H.264 movies in the format the PSP's sceMp4
# plays, differing only in what their SPS says about reference frames and picture buffering.
#
# Each frame shows its own number in binary, as eight bars across it (white = bit set), so the test
# can tell exactly which picture the decoder handed back, whatever the colour conversion.
#
# x264 always writes max_dec_frame_buffering equal to max_num_ref_frames, so to tell those apart
# some streams get their VUI's bitstream_restriction rewritten (or dropped) afterwards: the SPS is
# cut at that flag, located with ffmpeg's trace_headers, and a new tail written in its place.
#
# Needs ffmpeg with libx264 on PATH. Run from this directory.
import os
import re
import subprocess
import sys

FRAMES = 60
OUT_DIR = 'hold'


def source(size):
	# Eight bars across the picture, whatever its width.
	bar = int(size.split('x')[0]) // 8
	return ("nullsrc=s=%s:r=30000/1001,"
		"geq=lum='if(bitand(N\\,pow(2\\,floor(X/%d)))\\,235\\,16)':cb=128:cr=128" % (size, bar))


# The PSP format needs an audio track, so the movies get a silent one; avchold doesn't register it.
SILENCE_INPUT = ['-f', 'lavfi', '-i', 'anullsrc=r=44100:cl=stereo']
SILENCE_OUTPUT = ['-map', '0:v', '-map', '1:a', '-avoid_negative_ts', 'make_zero', '-c:a', 'aac', '-b:a', '32k', '-shortest',
	'-movflags', '+faststart', '-f', 'psp']


def run(args):
	subprocess.run(args, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def encode(path, x264params, fmt, size, level):
	src = source(size)
	video = ['-frames:v', str(FRAMES), '-c:v', 'libx264', '-profile:v', 'main', '-level', level,
		'-pix_fmt', 'yuv420p', '-b:v', '400k',
		'-x264-params', x264params + ':b-pyramid=none:weightp=0:repeat-headers=1:keyint=300:scenecut=0']
	if fmt == 'psp':
		run(['ffmpeg', '-y', '-f', 'lavfi', '-i', src] + SILENCE_INPUT + video + SILENCE_OUTPUT + [path])
	else:
		run(['ffmpeg', '-y', '-f', 'lavfi', '-i', src] + video + ['-an', '-f', fmt, path])


def mux(h264, mp4):
	run(['ffmpeg', '-y', '-r', '30000/1001', '-f', 'h264', '-i', h264] + SILENCE_INPUT
		+ ['-c:v', 'copy'] + SILENCE_OUTPUT + [mp4])


class Bits:
	def __init__(self):
		self.bits = []

	def u(self, n, v):
		for i in reversed(range(n)):
			self.bits.append((v >> i) & 1)

	def ue(self, v):
		v += 1
		n = v.bit_length()
		self.u(n - 1, 0)
		self.u(n, v)


def unescape(nal):
	out = bytearray()
	zeros = 0
	for b in nal:
		if zeros >= 2 and b == 3:
			zeros = 0
			continue
		out.append(b)
		zeros = zeros + 1 if b == 0 else 0
	return bytes(out)


def escape(rbsp):
	out = bytearray()
	zeros = 0
	for b in rbsp:
		if zeros >= 2 and b <= 3:
			out.append(3)
			zeros = 0
		out.append(b)
		zeros = zeros + 1 if b == 0 else 0
	return bytes(out)


def restriction_fields(h264):
	# Bit offset of bitstream_restriction_flag within the SPS NAL, and the values after it.
	trace = subprocess.run(['ffmpeg', '-i', h264, '-c', 'copy', '-bsf:v', 'trace_headers', '-frames:v', '1',
		'-f', 'null', '-'], capture_output=True, text=True).stderr
	fields = {}
	offset = None
	for line in trace.splitlines():
		m = re.search(r'\]\s+(\d+)\s+(\w+)\s+[01]+ = (\d+)', line)
		if not m:
			continue
		if m.group(2) == 'bitstream_restriction_flag' and offset is None:
			offset = int(m.group(1))
		elif offset is not None and m.group(2) not in fields:
			fields[m.group(2)] = int(m.group(3))
		if m.group(2) == 'max_dec_frame_buffering':
			break
	return offset, fields


def rewrite_sps(h264, out, restriction):
	# restriction: None to drop bitstream_restriction, or (max_num_reorder_frames, max_dec_frame_buffering).
	offset, f = restriction_fields(h264)
	data = open(h264, 'rb').read()
	starts = [m.start() for m in re.finditer(b'\x00\x00\x01', data)]
	result = bytearray()
	for i, s in enumerate(starts):
		end = starts[i + 1] if i + 1 < len(starts) else len(data)
		nal = data[s + 3:end]
		# A 4-byte start code leaves a trailing zero on the NAL before it.
		trailing = b''
		if i + 1 < len(starts) and nal.endswith(b'\x00'):
			nal = nal[:-1]
			trailing = b'\x00'
		if (nal[0] & 0x1F) == 7:
			rbsp = unescape(nal)
			bits = Bits()
			for k in range(offset):
				bits.bits.append((rbsp[k // 8] >> (7 - k % 8)) & 1)
			if restriction is None:
				bits.u(1, 0)
			else:
				bits.u(1, 1)
				bits.u(1, f['motion_vectors_over_pic_boundaries_flag'])
				bits.ue(f['max_bytes_per_pic_denom'])
				bits.ue(f['max_bits_per_mb_denom'])
				bits.ue(f['log2_max_mv_length_horizontal'])
				bits.ue(f['log2_max_mv_length_vertical'])
				bits.ue(restriction[0])
				bits.ue(restriction[1])
			bits.u(1, 1)  # rbsp_stop_one_bit
			while len(bits.bits) % 8:
				bits.bits.append(0)
			rbsp = bytes(int(''.join(map(str, bits.bits[k:k + 8])), 2) for k in range(0, len(bits.bits), 8))
			nal = escape(rbsp)
		result += b'\x00\x00\x01' + nal + trailing
	open(out, 'wb').write(result)


# name, x264 params, VUI rewrite (None = keep x264's, 'drop', or (reorder, dec_frame_buffering)),
# and optionally the size and level (480x272 at 3.0 otherwise). avchold.c has the same list.
STREAMS = [
	('ref1', 'ref=1:bframes=0', None),
	('ref2', 'ref=2:bframes=0', None),
	('ref3', 'ref=3:bframes=0', None),
	('ref4', 'ref=4:bframes=0', None),
	('ref5', 'ref=5:bframes=0', None),
	('ref3norestrict', 'ref=3:bframes=0', 'drop'),
	('ref1norestrict', 'ref=1:bframes=0', 'drop'),
	('ref1buf3', 'ref=1:bframes=0', (0, 3)),
	('ref3reorder2', 'ref=3:bframes=0', (2, 3)),
	('ref3buf4', 'ref=3:bframes=0', (0, 4)),
	('ref3b1', 'ref=3:bframes=1', None),
	('ref3b2', 'ref=3:bframes=2', None),
	('s144', 'ref=1:bframes=0', None, '144x80', '2.1'),
	('s240', 'ref=1:bframes=0', None, '240x136', '3.0'),
	('s320', 'ref=1:bframes=0', None, '320x176', '3.0'),
	('s480l21', 'ref=1:bframes=0', None, '480x272', '2.1'),
	# What Sony's encoder writes and x264 doesn't by default: access unit delimiters, HRD
	# parameters with buffering period and picture timing SEI, and pic_struct.
	('ref1aud', 'ref=1:bframes=0:aud=1', None),
	('ref1hrd', 'ref=1:bframes=0:nal-hrd=vbr:vbv-maxrate=1000:vbv-bufsize=2000', None),
	('ref1pic', 'ref=1:bframes=0:pic-struct=1', None),
	('ref1hrdpic', 'ref=1:bframes=0:aud=1:nal-hrd=vbr:vbv-maxrate=1000:vbv-bufsize=2000:pic-struct=1', None),
	('ref3b1hrd', 'ref=3:bframes=1:nal-hrd=vbr:vbv-maxrate=1000:vbv-bufsize=2000:pic-struct=1', None),
]


def main():
	os.makedirs(OUT_DIR, exist_ok=True)

	for entry in STREAMS:
		name, params, vui = entry[:3]
		size, level = entry[3:] if len(entry) > 3 else ('480x272', '3.0')
		mp4 = os.path.join(OUT_DIR, name + '.mp4')
		if vui is None:
			# Straight to MP4, which keeps the composition offsets B-frames need.
			encode(mp4, params, 'psp', size, level)
		else:
			# Rewritten streams have no B-frames, so a raw stream loses nothing on the way.
			raw = os.path.join(OUT_DIR, name + '.h264')
			encode(raw, params, 'h264', size, level)
			fixed = raw + '.fixed'
			rewrite_sps(raw, fixed, None if vui == 'drop' else vui)
			mux(fixed, mp4)
			os.remove(raw)
			os.remove(fixed)
		print(name)


if __name__ == '__main__':
	sys.exit(main())
