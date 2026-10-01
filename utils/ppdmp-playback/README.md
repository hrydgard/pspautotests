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

When the replay is done, the display shows the result (the dump's last display framebuffer, or the
last framebuffer it drew to). PSPLink clears the screen when the program exits, so `--hold-ms=1500`
keeps it up for a while first. `--display=ADDR,STRIDE,FMT` (hex address, e.g. `04000000,512,3`)
shows another buffer instead, such as an offscreen render target.

`run.py` does all of this from the host, for one or more dumps (`.ppdmp`, or a zip holding one, as in
the frametests repo), and saves what the PSP displays as `NAME-psp.png`:
```sh
python3 utils/ppdmp-playback/run.py --out /tmp/shots dumps/bug123.zip
python3 utils/ppdmp-playback/run.py --headless ../build/PPSSPPHeadless dumps/bug123.zip
```
With `--headless`, it also renders each dump in PPSSPPHeadless (`--graphics=software` by default),
saves `NAME-ppsspp.png` and prints the MSE between the two. Each result stays on the PSP's screen for
1.5 seconds (`--hold=SECONDS` to change it). It builds `playback.prx` first, starts `usbhostfs_pc` if
needed, and runs the dumps one after another.

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
