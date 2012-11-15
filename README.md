  * Original and outdated svn repository: http://code.google.com/p/pspautotests/
  * New and updated git repository: https://github.com/hrydgard/pspautotests

A repository of PSP programs performing several tests on the PSP platform.

  * It will allow people to see how to use some obscure-newly-discovered APIs and features
  * It will allow PSP emulators to avoid some regressions while performing refactorings and to have a reference while implementing APIs

The main idea behind this is having several files per test unit:
  * _file_*.expected* - File with the expected Kprintf's output, preferably from a real PSP
  * _file_*.elf* - The program that will call Kprintf syscall in order to generate an output
  * _file_*.input* - Optional file specifying automated actions that should simulate user interaction: pressing a key, releasing a key, selecting a file on the save selector, waiting for a function (for example a vsync) to call before continuing...



How to build and use
--------------------

If you just want to run the tests, you just need to run your emulator on the PRX-es and compare with the .expected
files. PPSSPP has a convenient script for this called test.py.

If you want to change tests, you'll need to read the rest.

* First you need to install the homebrew PSPSDK. On Windows I'd recommend MinPSPW, http://www.jetdrone.com/minpspw.
* Then, you need to install PSPLink on your PSP. Upgrade your PSP to 6.60 and install CFW if you haven't already.
* Then copy the OE version of PSPLink to PSP/GAME on the PSP, and run it.

Alright, PSP prepared, now for the PC. I had a lot of trouble connecting to PSPLink on Windows 7 x64 and with modern FW, but I figured it out. Here's what you have to do:

Before installing the psplink driver, you have to boot windows using F8 at bootup and select "Disable driver signing verification" or something.

When you then connect your PSP running PSPLink to your PC, the PC will ask for a driver first time, choose the one that comes with the pspsdk. Then, in one cmd terminal, sitting in the emu directory:

> usbhostfs_pc -b 3000

Then in another terminal:

> pspsh -p 3000

Now you have full access to the PSP from this prompt. test.py simply calls pspsh with a single line script file to execute each test on the PSP and copy the output file to the correct directory.



Old notes:

However, here I had one more problem: Modern PSPLink won't load ELF files. You have to build everything as PRX (encrypted elf). So I had to switch pspautotests over to use pspsdk makefiles and set BUILD_PRX = 1. This exposed one more problem : by default when you build a PRX, it sets the heap size to 64k. So I added a workaround for that in common.c.

Finally, everything works :)




@TODO: Maybe join .expected and .input file in a single .test file?

Random Ideas for .test file:
```
EXPECTED:CALL(sceDisplay.sceDisplayWaitVblank)
ACTION:BUTTON_PRESS(CROSS)
EXPECTED:OUTPUT('CROSS Pressed')
EXPECTED:CALL(sceDisplay.sceDisplayWaitVblank)
ACTION:BUTTON_RELEASE(CROSS)
```
