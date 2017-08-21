#include <common.h>
#include <pspdisplay.h>
#include <pspthreadman.h>

extern "C" int sceDisplayIsVsync();

extern "C" int HAS_DISPLAY;

void testIsState(const char *title) {
	int fg = sceDisplayIsForeground();
	int vsync = sceDisplayIsVsync();
	int vblank = sceDisplayIsVblank();
	checkpoint("%s: foreground=%d, vsync=%d, vblank=%d", title, fg, vsync, vblank);
}

static void testStateScenarios() {
	sceDisplayWaitVblankStart();
	testIsState("  After start");

	sceKernelDelayThread(1000);
	testIsState("  After vblank end");

	// Poll for sync after vblank start.
	sceDisplayWaitVblankStart();
	s64 start = sceKernelGetSystemTimeWide();
	s64 end = start + 1000;
	bool sawVsync = false;
	bool vblankTimingGood = true;
	while (sceKernelGetSystemTimeWide() < end) {
		int vsyncResult = sceDisplayIsVsync();
		if (vsyncResult) {
			testIsState("  During vsync");
			sawVsync = true;

			int untilVsync = (int)(sceKernelGetSystemTimeWide() - start);
			int untilVblankEnd = 0;
			while (sceKernelGetSystemTimeWide() < end) {
				if (!sceDisplayIsVblank()) {
					untilVblankEnd = (int)(sceKernelGetSystemTimeWide() - start);
					break;
				}
			}

			if (untilVsync < 500 || untilVsync > 700) {
				checkpoint("  Vsync did not occur when expected (should be around 631.5us from vblank start)");
				vblankTimingGood = false;
			}
			if (untilVblankEnd < 600 || untilVblankEnd > 800) {
				checkpoint("  Vblank did not take as long as expected (should be around 731.5us from vblank start)");
				vblankTimingGood = false;
			}

			break;
		}
	}

	if (!sawVsync) {
		checkpoint("  Never received vsync");
	} else if (vblankTimingGood) {
		checkpoint("  Vblank timing checked out");
	}
}

extern "C" int main(int argc, char *argv[]) {
	// Let's not bother debugging to the display, since we'll just mess it up anyway.
	HAS_DISPLAY = 0;

	sceDisplaySetMode(0, 480, 272);
	sceDisplaySetFrameBuf(0, 0, 0, 1);
	sceDisplaySetFrameBuf(0, 0, 0, 0);

	checkpointNext("While off:");
	sceDisplayWaitVblankStart();
	testStateScenarios();

	sceDisplaySetFrameBuf((void *)0x04000000, 512, 3, 1);
	sceDisplaySetFrameBuf((void *)0x04000000, 512, 3, 0);

	checkpointNext("While on:");
	sceDisplayWaitVblankStart();
	testStateScenarios();

	sceDisplaySetFrameBuf(0, 0, 0, 1);
	checkpointNext("Latched off");
	testIsState("  Latched off");

	return 0;
}