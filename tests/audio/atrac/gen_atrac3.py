#!/usr/bin/env python3
# Writes synthetic Atrac3 test data (no game audio in the repo):
#
#   atrac3_c0_mono.at3   70 frames of 0xC0 bytes, each one mono sound unit, behind a 2-channel
#                        normal-stereo header (the header LocoRoco 2 writes for every track it
#                        streams). Each unit runs past byte 96, so reading the frame as two
#                        96-byte channels breaks, as it does for the game's MuiMui house music.
#   atrac3_180_stereo.raw  4 raw frames of 0x180 bytes, normal stereo, different channels.
#
# The units are hand-built: sound unit id 0x28, one QMF band, no gain control, no tonal components,
# and a constant-length-coded spectrum (6 bits per coefficient) over the first 12 subbands.
import math
import struct

class BitWriter:
    def __init__(self):
        self.bits = []

    def put(self, value, n):
        for i in range(n - 1, -1, -1):
            self.bits.append((value >> i) & 1)

    def bytes(self, size):
        assert len(self.bits) <= size * 8, len(self.bits)
        bits = self.bits + [0] * (size * 8 - len(self.bits))
        return bytes(int(''.join(map(str, bits[i:i + 8])), 2) for i in range(0, len(bits), 8))

SUBBANDS = [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128]

def sound_unit(frame, size, freq, amp):
    w = BitWriter()
    w.put(0x28, 6)   # sound unit id
    w.put(0, 2)      # coded QMF bands - 1
    w.put(0, 3)      # gain control: no points for band 0
    w.put(0, 5)      # no tonal components
    w.put(len(SUBBANDS) - 2, 5)  # coded subbands - 1
    w.put(1, 1)      # constant length coding
    for _ in SUBBANDS[:-1]:
        w.put(7, 3)  # selector 7: 6-bit signed mantissas
    for _ in SUBBANDS[:-1]:
        w.put(40, 6)  # scale factor index
    for k in range(SUBBANDS[-1]):
        # A peak around bin 'freq' that moves a little every frame, plus a deterministic texture.
        v = amp * math.exp(-((k - freq - (frame % 8)) ** 2) / 8.0) + 3 * math.sin(k * 0.7 + frame)
        m = max(-31, min(31, int(round(v))))
        w.put(m & 0x3F, 6)
    return w.bytes(size)

def header(frame_size, data_size):
    # Laid out as LocoRoco 2's z_un_08aaaab4 builds it: 2 channels and normal stereo, whatever the
    # frame size.
    h = bytearray(0x4C)
    h[0:4] = b'RIFF'
    struct.pack_into('<I', h, 0x04, data_size + 0x44)
    h[0x08:0x10] = b'WAVEfmt '
    struct.pack_into('<IHHII', h, 0x10, 0x20, 0x270, 2, 44100, (frame_size * 44100 + 0x200) >> 10)
    struct.pack_into('<HH', h, 0x20, frame_size, 0)
    struct.pack_into('<HHIHHHH', h, 0x24, 0x0E, 1, 0x1000, 0, 0, 1, 0)
    h[0x34:0x38] = b'fact'
    # The game declares 200000 frames whatever the real length (it streams), and the firmware
    # accepted nothing else in testing, so do the same.
    struct.pack_into('<III', h, 0x38, 8, 200000 * 1024 - 0x445, 0x400)
    h[0x44:0x48] = b'data'
    struct.pack_into('<I', h, 0x48, frame_size * 200000)
    return bytes(h)

def main():
    frames = []
    for i in range(70):
        unit = sound_unit(i, 0xC0, 20 + (i // 10) * 6, 28)
        # A stereo split would start its second channel at byte 96, in the middle of this unit.
        assert unit[96] & 0xFC != 0xA0
        frames.append(unit)
    data = b''.join(frames)
    with open('atrac3_c0_mono.at3', 'wb') as f:
        f.write(header(0xC0, len(data)) + data)

    stereo = b''.join(sound_unit(i, 0xC0, 16, 26) + sound_unit(i + 3, 0xC0, 40, 20) for i in range(4))
    with open('atrac3_180_stereo.raw', 'wb') as f:
        f.write(stereo)

if __name__ == '__main__':
    main()
