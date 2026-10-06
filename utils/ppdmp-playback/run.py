#!/usr/bin/env python3
# Replays PPSSPP GE frame dumps on a real PSP through PSPLink, and saves what the PSP displays
# afterwards as a PNG. Optionally renders the same dump in PPSSPPHeadless and compares the two.
#
# usbhostfs_pc must serve the pspautotests root (it's started here if it isn't running), and the
# PSP must be in PSPLink. Dumps run strictly one after another: there is one PSP.

import argparse
import os
import shutil
import socket
import subprocess
import sys
import time
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
PRX = 'utils/ppdmp-playback/playback.prx'
STAGED = '__framedump.ppdmp'
FINISHFILE = '__testfinish.txt'
OUTFILE = '__testoutput.txt'
ERRFILE = '__testerror.txt'
SHOTFILE = '__screenshot.bmp'
DEPTHFILE = '__depth.bin'
RECONNECT_TIMEOUT = 10


def wait_until(predicate, timeout, interval=0.2):
	end = time.time() + timeout
	while time.time() < end:
		if predicate():
			return True
		time.sleep(interval)
	return False


def hostfs_is_ready(port):
	try:
		with socket.create_connection(('127.0.0.1', port), timeout=0.5):
			return True
	except OSError:
		return False


def pspsh(port, command, timeout):
	try:
		r = subprocess.run(['pspsh', '-p', str(port), '-e', command], capture_output=True, timeout=timeout)
		return r.stdout.decode('utf-8', 'replace')
	except subprocess.TimeoutExpired:
		return None


def pspsh_is_ready(port):
	out = pspsh(port, 'ls', 1.0)
	return out is not None and out.count('\n') > 2


def read_dump(path):
	# A .ppdmp, or a zip holding one (as in the frametests repo).
	if zipfile.is_zipfile(path):
		with zipfile.ZipFile(path) as z:
			names = [n for n in z.namelist() if n.lower().endswith('.ppdmp')]
			if len(names) != 1:
				raise ValueError('%s: expected one .ppdmp inside, found %d' % (path, len(names)))
			return z.read(names[0])
	with open(path, 'rb') as f:
		return f.read()


def dump_name(path):
	name = os.path.basename(path)
	for ext in ('.zip', '.ZIP', '.ppdmp'):
		if name.endswith(ext):
			name = name[:-len(ext)]
	return name


def save_png(bmp_path, png_path):
	from PIL import Image
	# The screenshot is 512 wide (the buffer stride); the display shows 480.
	Image.open(bmp_path).convert('RGB').crop((0, 0, 480, 272)).save(png_path)


def mse(a_path, b_path):
	import numpy as np
	from PIL import Image
	a = np.asarray(Image.open(a_path).convert('RGB').crop((0, 0, 480, 272)), dtype=np.float64)
	b = np.asarray(Image.open(b_path).convert('RGB').crop((0, 0, 480, 272)), dtype=np.float64)
	return float(np.mean((a - b) ** 2))


def run_on_psp(args, data):
	for f in (FINISHFILE, OUTFILE, ERRFILE, SHOTFILE, DEPTHFILE):
		if os.path.exists(f):
			os.unlink(f)
	with open(STAGED, 'wb') as f:
		f.write(data)
	try:
		if not pspsh_is_ready(args.port):
			print('Waiting for the PSP to connect...')
			if not wait_until(lambda: pspsh_is_ready(args.port), RECONNECT_TIMEOUT):
				print('ERROR: the PSP is not connected (is it in PSPLink?)')
				return False
		command = '%s host0:/%s --hold-ms=%d' % (PRX, STAGED, int(args.hold * 1000))
		if args.start is not None:
			command += ' --start=%d' % args.start
		if args.end is not None:
			command += ' --end=%d' % args.end
		if args.progress:
			command += ' --progress=%d' % args.progress
		if args.cmds:
			command += ' --cmds=%d' % args.cmds
		if args.trace_from:
			command += ' --trace-from=%d' % args.trace_from
		if args.no_depth:
			command += ' --no-depth'
		if args.display:
			command += ' --display=%s' % args.display
		pspsh(args.port, command, 5.0)
		# pspsh returns once the module is started; the finish file marks its exit.
		if not wait_until(lambda: os.path.exists(FINISHFILE), args.timeout + args.hold, 0.1):
			print('ERROR: timed out after %d seconds, resetting the PSP' % (args.timeout + args.hold))
			if os.path.exists(OUTFILE):
				# The last lines show how far it got (with --progress or --trace-from).
				print('\n'.join(open(OUTFILE, 'rt', errors='replace').read().strip().split('\n')[-12:]))
			pspsh(args.port, 'reset', 5.0)
			return False
		output = open(OUTFILE, 'rt', errors='replace').read() if os.path.exists(OUTFILE) else ''
		if 'VALID: 1' not in output or 'RUN: 1' not in output:
			print('ERROR: the replay failed:\n' + output.strip())
			return False
		if not os.path.exists(SHOTFILE):
			print('ERROR: no screenshot was written')
			return False
		return True
	finally:
		os.unlink(STAGED)


def run_headless(args, data, png_path):
	tmp = os.path.join(ROOT, '__framedump_headless.ppdmp')
	with open(tmp, 'wb') as f:
		f.write(data)
	try:
		subprocess.run([args.headless, tmp, '--graphics=' + args.graphics, '--screenshot-save=' + png_path,
			'--timeout-wall=30'], capture_output=True, timeout=60)
	finally:
		os.unlink(tmp)
	return os.path.exists(png_path)


def main():
	parser = argparse.ArgumentParser(description='Replay PPSSPP GE frame dumps on a PSP over PSPLink.')
	parser.add_argument('dumps', nargs='+', help='.ppdmp files, or zips holding one')
	parser.add_argument('--out', default='.', help='directory for NAME-psp.png (and NAME-ppsspp.png)')
	parser.add_argument('--hold', type=float, default=1.5, help='seconds to show the result on the PSP before exiting')
	parser.add_argument('--start', type=int, help='first primitive to draw')
	parser.add_argument('--end', type=int, help='last primitive to draw')
	parser.add_argument('--timeout', type=int, default=60, help='seconds to wait for a replay')
	parser.add_argument('--progress', type=int, help='print a progress line every N dump commands, to find a hang')
	parser.add_argument('--cmds', type=int, help='only replay the first N dump commands, to bisect a hang')
	parser.add_argument('--trace-from', type=int, help='print every dump command from this index on')
	parser.add_argument('--no-depth', action='store_true', help='skip the depth buffer readback')
	parser.add_argument('--display', help='addr,stride,format (hex addr) of a buffer to show and screenshot instead of the display')
	parser.add_argument('--port', type=int, default=3000)
	parser.add_argument('--headless', help='PPSSPPHeadless to render each dump with too, printing the MSE')
	parser.add_argument('--graphics', default='software', help='backend for --headless')
	parser.add_argument('--no-make', action='store_true', help='skip rebuilding playback.prx')
	parser.add_argument('--settle', type=float, default=1.0, help='seconds to wait between replays: the finish file is written before the module has exited, and starting the next one too soon fails')
	args = parser.parse_args()

	dumps = [os.path.abspath(d) for d in args.dumps]
	out_dir = os.path.abspath(args.out)
	if args.headless:
		args.headless = os.path.abspath(args.headless)
	os.makedirs(out_dir, exist_ok=True)
	os.chdir(ROOT)

	if not args.no_make and subprocess.run(['make'], cwd=os.path.dirname(PRX), capture_output=True).returncode != 0:
		print('ERROR: building playback.prx failed')
		return 1

	hostfs = None
	if not hostfs_is_ready(args.port):
		hostfs = subprocess.Popen(['usbhostfs_pc', '-b', str(args.port)], stdout=subprocess.DEVNULL)
		if not wait_until(lambda: hostfs_is_ready(args.port), RECONNECT_TIMEOUT):
			print('ERROR: could not start usbhostfs_pc')
			return 1

	failures = 0
	try:
		for i, path in enumerate(dumps):
			if i > 0:
				time.sleep(args.settle)
			name = dump_name(path)
			print('%s:' % name, end=' ', flush=True)
			data = read_dump(path)
			if not run_on_psp(args, data):
				failures += 1
				continue
			psp_png = os.path.join(out_dir, name + '-psp.png')
			save_png(SHOTFILE, psp_png)
			if os.path.exists(DEPTHFILE):
				shutil.copyfile(DEPTHFILE, os.path.join(out_dir, name + '-psp-depth.bin'))
			line = psp_png
			if args.headless:
				ppsspp_png = os.path.join(out_dir, name + '-ppsspp.png')
				if run_headless(args, data, ppsspp_png):
					line += '  MSE vs PPSSPP (%s): %.3f' % (args.graphics, mse(psp_png, ppsspp_png))
				else:
					line += '  (PPSSPPHeadless wrote no screenshot)'
			print(line)
	finally:
		if hostfs is not None:
			hostfs.terminate()
	return 1 if failures else 0


if __name__ == '__main__':
	sys.exit(main())
