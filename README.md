  * Original and outdated svn repository: http://code.google.com/p/pspautotests/
  * New and updated git repository: https://github.com/hrydgard/pspautotests

A repository of PSP programs performing several tests on the PSP platform.

  * It will allow people to see how to use some obscure-newly-discovered APIs and features
  * It will allow PSP emulators to avoid some regressions while performing refactorings and to have a reference while implementing APIs

The main idea behind this is having several files per test unit:
  * _file_*.expected* - File with the expected Kprintf's output
  * _file_*.elf* - The program that will call Kprintf syscall in order to generate an output
  * _file_*.input* - Optional file specifying automated actions that should simulate user interaction: pressing a key, releasing a key, selecting a file on the save selector, waiting for a function (for example a vsync) to call before continuing...

@TODO: Maybe join .expected and .input file in a single .test file?

Random Ideas for .test file:
```
EXPECTED:CALL(sceDisplay.sceDisplayWaitVblank)
ACTION:BUTTON_PRESS(CROSS)
EXPECTED:OUTPUT('CROSS Pressed')
EXPECTED:CALL(sceDisplay.sceDisplayWaitVblank)
ACTION:BUTTON_RELEASE(CROSS)
```