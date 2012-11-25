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
  c = Command([PSPSH, "-p", str(PORT), "-e", "ls"]);
  c.run(0.5)
  return c.output.count('\n') > 2

def gen_test(test):
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

    success = wait_until(pspsh_is_ready, 5, 0.2);

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
    shutil.copyfile(OUTFILE, expected_path)
    print "Expected file written: " + expected_path
  else:
    print "ERROR: No or empty " + OUTFILE + " was written, can't write .expected"

def main():
  args = sys.argv[1:]
  
  tests = tests_to_generate
  if len(args):
    tests = args

  for test in tests:
    gen_test(test.replace("\\", "/"))

main()
