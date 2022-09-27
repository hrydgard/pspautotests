Playback
========

This program runs GE frame dumps produced by PPSSPP on a PSP.

Usage
-----

By default, running the prx will look for the file `host0:/framedump.ppdmp`, execute everything in it, and produce a screenshot at `host0:/__screenshot.bmp`.

It can be run using psplink:
```sh
pspsh -p 3000 -e utils/ppdmp-playback/playback.prx
```

Arguments can be passed to run a different dump file, or draw a subset of the primitives in the frame dump.  For example:
```sh
pspsh -p 3000 -e "utils/ppdmp-playback/playback.prx host0:/framedumps/bug123.ppdmp --start=1 --end=1000"
```

Note that pspsh expects the command and its arguments all to be together inside the quotes.

Building
--------

To build this tool, simply run `make` while the pspsdk is available on the `PATH`.  This will create a `playback.prx` which is the program.

Troubleshooting
---------------

Some tips if you run into problems:

 * To enable host0: communication, you need to run `usbhostfs_pc -b 3000`.  The directory you run this in becomes the root that everything is relative to.
 * You can use another port than 3000, it's just an example.  This is a TCP port used for communication between `pspsh` and `usbhostfs_pc`.
 * Some frame dumps may run much slower than the original rendering; this does not approximate rendering speed proeprly.  Give it time.
 * In some cases, the wrong display may be output if there's a problem with the frame dump.  You can call `sceDisplaySetFrameBuf()` with your own framebuffer and then recompile if necessary.
 * Sometimes, a frame dump may overwrite previous rendering.  This tends to happen in `Replay::Framebuf()` when the rendering wasn't detected properly.  You can temporarily hardcode addresses not to copy there.
 * Very large frame dumps will not execute properly because there's no memory management; the entire decompressed frame dump most fit in PSP RAM.
