#!/usr/bin/env python3
# Records a two-console test: runs NAME_host on one PSP and NAME_client on the other, at the same
# time, and writes NAME_host.expected and NAME_client.expected.
#
# Setup, once, with both PSPs connected and running PSPLink:
#   cd pspautotests && usbhostfs_pc -b 3000 &
#   cd pspautotests/tests/net/adhoc/pair && usbhostfs_pc -b 4000 . &
# Each usbhostfs_pc takes 8 ports from its base, and whichever PSP the other one hasn't claimed. Their
# host0: directories differ, so the two consoles' output files don't collide. Which console ends up
# on which port isn't up to you, so check with `pspsh -p PORT -e pspver` and pass --host-port to run
# the host on the console the other recordings come from.
#
# Usage, from anywhere: python3 tests/net/adhoc/pair/record.py [--host-port 3000|4000] basic

import argparse
import os
import shutil
import subprocess
import sys
import time

PAIR_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(PAIR_DIR, "..", "..", "..", ".."))
# Port -> (host0: directory, path of this directory as seen from that host0:)
BRIDGES = {
	3000: (ROOT, "tests/net/adhoc/pair/"),
	4000: (PAIR_DIR, "./"),
}
TIMEOUT = 90
OUTPUT_FILES = ("__testoutput.txt", "__testerror.txt", "__testfinish.txt")


def pspsh(port, cmd, timeout=10):
	try:
		return subprocess.run(["pspsh", "-p", str(port), "-e", cmd], capture_output=True, timeout=timeout).stdout.decode("utf-8", "replace")
	except subprocess.TimeoutExpired:
		return None


def collect(port, role, name):
	root = BRIDGES[port][0]
	finish = os.path.join(root, "__testfinish.txt")
	error = os.path.join(root, "__testerror.txt")
	if not os.path.exists(finish):
		print("ERROR: %s (port %d) didn't finish" % (role, port))
		return False
	if os.path.exists(error) and os.path.getsize(error) > 0:
		print("ERROR: %s (port %d) wrote to stderr" % (role, port))
		return False
	expected = os.path.join(PAIR_DIR, "%s_%s.expected" % (name, role))
	shutil.copy(os.path.join(root, "__testoutput.txt"), expected)
	print("Expected file written: " + os.path.relpath(expected, ROOT))
	return True


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("--host-port", type=int, default=3000, choices=sorted(BRIDGES))
	parser.add_argument("name")
	args = parser.parse_args()
	host_port = args.host_port
	client_port = [p for p in BRIDGES if p != host_port][0]

	for port in (host_port, client_port):
		out = pspsh(port, "ls")
		if not out or "Listing" not in out:
			print("No PSP answering on port %d" % port)
			return 1
		for f in OUTPUT_FILES:
			path = os.path.join(BRIDGES[port][0], f)
			if os.path.exists(path):
				os.unlink(path)

	# The client scans for up to 15 seconds, so it can start first; the host follows straight away.
	for port, role in ((client_port, "client"), (host_port, "host")):
		out = pspsh(port, "%s%s_%s.prx" % (BRIDGES[port][1], args.name, role))
		print("%s (port %d): %s" % (role, port, (out or "no answer").strip()))

	deadline = time.time() + TIMEOUT
	finishes = [os.path.join(BRIDGES[p][0], "__testfinish.txt") for p in (host_port, client_port)]
	while time.time() < deadline and not all(os.path.exists(f) for f in finishes):
		time.sleep(0.2)

	ok = collect(host_port, "host", args.name)
	ok = collect(client_port, "client", args.name) and ok
	# Both test programs stay loaded, and a hung one wedges PSPLink, so reset both either way.
	pspsh(host_port, "reset")
	pspsh(client_port, "reset")
	return 0 if ok else 1


if __name__ == "__main__":
	sys.exit(main())
