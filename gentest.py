# Utility to make it slightly easier to get the results from a test back
# and put it as an "expected" file.

import sys
import io
import os
import subprocess
import threading
import shutil
import time

PSPSH = "pspsh"
MAKE = "make"
TEST_ROOT = "tests/"
PORT = 3000
OUTFILE = "__testoutput.txt"
OUTFILE2 = "__testerror.txt"
FINISHFILE = "__testfinish.txt"
TIMEOUT = 5


tests_to_generate = [
  "cpu/cpu/cpu",
  "cpu/icache/icache",
  "cpu/lsu/lsu",
  "cpu/fpu/fpu",
]


class Command(object):
  def __init__(self, cmd):
    self.cmd = cmd
    self.process = None
    self.output = None
    self.timeout = False

  def run(self, timeout):
    def target():
      self.process = subprocess.Popen(self.cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
      self.output, _ = self.process.communicate()

    thread = threading.Thread(target=target)
    thread.start()

    thread.join(timeout)
    if thread.is_alive():
      self.timeout = True
      self.process.terminate()
      thread.join()

def wait_until(predicate, timeout, interval):
  mustend = time.time() + timeout
  while time.time() < mustend:
    if predicate(): return True
    time.sleep(interval)
  return False

def pspsh_is_ready():
  c = Command([PSPSH, "-p", str(PORT), "-e", "ls"])
  c.run(0.5)
  return c.output.count("\n") > 2

def init():
  if not os.path.exists(TEST_ROOT + "../common/libcommon.a"):
    print "Please install the pspsdk and run make in common/"
    if not ("-k" in sys.argv or "--keep" in sys.argv):
      sys.exit(1)

def gen_test(test, args):
  if not ("-k" in args or "--keep" in args):
    olddir = os.getcwd()
    os.chdir(TEST_ROOT + os.path.dirname(test))

    make_target = "all"
    if "-r" in args or "--rebuild" in args:
      make_target = "rebuild"

    make_result = os.system("%s MAKE=\"%s\" %s" % (MAKE, MAKE, make_target))
    os.chdir(olddir)

    # Don't run the test if make failed, let them fix it.
    if make_result > 0:
      sys.exit(make_result)

  print("Running test " + test + " on the PSP...")

  if os.path.exists(OUTFILE):
    os.unlink(OUTFILE)
  if os.path.exists(OUTFILE2):
    os.unlink(OUTFILE2)
  if os.path.exists(FINISHFILE):
    os.unlink(FINISHFILE)

  prx_path = TEST_ROOT + test + ".prx"
  expected_path = TEST_ROOT + test + ".expected"

  if not os.path.exists(prx_path):
    print "You must compile the test into a PRX first (" + prx_path + ")"
    return

  # Wait for the PSP to reconnect after a previous test.
  if not pspsh_is_ready():
    print "Waiting for PSP to connect..."

    success = wait_until(pspsh_is_ready, 5, 0.2)

    # No good, it never came back.
    if not success:
      print "Please make sure the PSP is connected"
      print "On Windows, the usb driver must be installed"
      return

  # Okay, time to run the command.
  c = Command([PSPSH, "-p", str(PORT), "-e", prx_path])
  c.run(TIMEOUT)
  print c.output

  # PSPSH returns right away, though, so do the timeout here.
  wait_until(lambda: os.path.exists(FINISHFILE), TIMEOUT, 0.1)

  if not os.path.exists(FINISHFILE):
    print "ERROR: Test timed out after 5 seconds"

    # Reset the test, it's probably dead.
    os.system("%s -p %i -e reset" % (PSPSH, PORT))
  elif os.path.exists(OUTFILE2) and os.path.getsize(OUTFILE2) > 0:
    print "ERROR: Script produced stderr output"
  elif os.path.exists(OUTFILE) and os.path.getsize(OUTFILE) > 0:
    # Normalize line endings on windows to avoid spurious git warnings.
    if sys.platform == 'win32':
      expected_data = open(expected_path, "rt").read()
      open(expected_path, "wt").write(expected_data)
    else:
      shutil.copyfile(OUTFILE, expected_path)
    print "Expected file written: " + expected_path
  else:
    print "ERROR: No or empty " + OUTFILE + " was written, can't write .expected"

def main():
  init()
  tests = []
  args = []
  for arg in sys.argv[1:]:
    if arg[0] == "-":
      args.append(arg)
    else:
      tests.append(arg.replace("\\", "/"))

  if not tests:
    tests = tests_to_generate

  if "-h" in args or "--help" in args:
    print "Usage: %s [options] cpu/icache/icache rtc/rtc...\n" % (os.path.basename(sys.argv[0]))
    print "Tests should be found under %s and omit the .prx extension." % (TEST_ROOT)
    print "Automatically runs make in the test by default.\n"
    print "Options:"
    print "  -r, --rebuild         run make rebuild for each test"
    print "  -k, --keep            do not run make before tests"
    return

  for test in tests:
    gen_test(test, args)

main()
