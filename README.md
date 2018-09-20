# pspautotests

  * Original and outdated svn repository: http://code.google.com/p/pspautotests/
  * New and updated git repository: https://github.com/hrydgard/pspautotests

A repository of PSP programs performing several tests on the PSP platform.

  * It will allow people to see how to use some obscure-newly-discovered APIs and features
  * It will allow PSP emulators to avoid some regressions while performing refactorings and to have a reference while implementing APIs

The main idea behind this is having several files per test unit:
  * _file_*.expected* - File with the expected Kprintf's output, preferably from a real PSP
  * _file_*.prx* - The program that will call Kprintf syscall in order to generate an output
  * _file_*.input* - Optional file specifying automated actions that should simulate user interaction: pressing a key, releasing a key, selecting a file on the save selector, waiting for a function (for example a vsync) to call before continuing...


How to build and use
--------------------

If you just want to run the tests, you just need to run your emulator on the PRX files and compare with the .expected
files. PPSSPP has a convenient script for this called test.py.

If you want to change tests, you'll need to read the rest. This tutorial is focused on Windows but can probably be used on Linux and Mac too, you just don't need to install the driver there.

### Prerequisites:
  - A PSP with custom firmware installed (6.60 recommended)
  - A USB cable to use between your PC and PSP
  - PSPSDK installed (on Windows I'd recommend MinPSPW, https://sourceforge.net/projects/minpspw/.)

The rest of this tutorial will assume that you installed the PSPSDK in C:\pspsdk.

### Step 1: Install PSPLink on your PSP
  - Copy the OE version of PSPLink (C:\pspsdk\psplink\psp\oe\psplink) to PSP/GAME on the PSP.
  - Run it on your PSP from the game menu.

### Step 2: Prepare the PC

Tip: If you see PSP Type A, you've connected the PSP in "USB mode".  Disconnect, and run the PSPLINK game instead.

#### Windows 7 and later
 - Plug the PSP into your PC via USB while PSPLINK is running.
 - Use [Zadig](https://zadig.akeo.ie/) to install the libusb-win32 driver.
 - Make sure it says PSP Type B in Zadig and click Install Driver.

#### Windows XP / Vista / etc.
 - If you are on Vista x64, you may need to press F8 during boot up and select "Disable driver signing verification".  You'll have to do this each boot on Vista x64.
 - After boot, plug the PSP into your PC via USB while PSPLINK is running.
 - Go into Device Manager and select the PSP Type B device in the list.
 - Right click on "PSP Type B" -> Properties.
 - Select Update Driver and select "I have my own driver".
 - For the path, use C:\pspsdk\bin\driver or C:\pspsdk\bin\driver_x64 depending on your OS install.

#### Mac OS X
 - Use `brew install libusb-compat` to install libusb.
 - See here for pspsdk instructions: https://github.com/krzkaczor/psp-developer-guide/blob/master/pspsdk-installation.md

#### Linux
 - Install libusb and pspsdk: https://github.com/krzkaczor/psp-developer-guide/blob/master/pspsdk-installation.md

### Step 3: Add pspsdk to PATH
 - Add C:\pspsdk\bin (or equivalent) to your PATH if you haven't already got it.

You are now ready to roll!

### Running tests

In a command prompt in the directory that you want the PSP software to regard as "host0:/" (normally pspautotests/) if it tries to read files over the cable, type the following:

> cd pspautotests<br />
> usbhostfs_pc -b 3000

Then in a separate command prompt:

> pspsh -p 3000

If you now don't see a host0:/ prompt, something is wrong. Most likely the driver has not loaded correctly. If the port 3000 happened to be taken (usbhostfs_pc would have complained), try another port number.

Now you have full access to the PSP from this prompt. You can use gentest.py to run tests (e.g. `gentest.py misc/testgp`) and update the .expected files.

You can run executables on the PSP that reside on the PC directly from within this the pspsh shell, just cd to the directory and run ./my_program.prx.

Note that you CAN'T run ELF files on modern firmware, you MUST build as .PRX. To do this, set BUILD_PRX = 1 in your makefile.

Also, somewhere in your program, add the following line to get a proper heap size:

unsigned int sce_newlib_heap_kb_size = -1;

For some probably historical reason, by default PSPSDK assumes that you want a 64k heap when you build a PRX.


TODO
----
Maybe join .expected and .input file in a single .test file?

Random Ideas for .test file:
```
EXPECTED:CALL(sceDisplay.sceDisplayWaitVblank)
ACTION:BUTTON_PRESS(CROSS)
EXPECTED:OUTPUT('CROSS Pressed')
EXPECTED:CALL(sceDisplay.sceDisplayWaitVblank)
ACTION:BUTTON_RELEASE(CROSS)
```
