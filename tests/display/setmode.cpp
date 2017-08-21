#include <common.h>
#include <pspdisplay.h>

extern "C" int HAS_DISPLAY;

void testSetMode(const char *title, int mode, int w, int h) {
	int result = sceDisplaySetMode(mode, w, h);
	int newMode, newW, newH;
	sceDisplayGetMode(&newMode, &newW, &newH);

	if (result < 0) {
		checkpoint("%s: Failed (%08x), mode=%d, %dx%d", title, result, newMode, newW, newH);
	} else {
		checkpoint("%s: OK (%08x), mode=%d, %dx%d", title, result, newMode, newW, newH);
	}
}

extern "C" int main(int argc, char *argv[]) {
	// Let's not bother debugging to the display, since we'll just mess it up anyway.
	HAS_DISPLAY = 0;

	checkpointNext("Resolutions:");
	testSetMode("  Zero", 0, 0, 0);
	testSetMode("  Normal", 0, 480, 272);
	testSetMode("  512", 0, 512, 272);
	testSetMode("  Twice", 0, 480, 272);
	testSetMode("  Large", 0, 480 * 2, 272 * 2);
	testSetMode("  Small", 0, 480 / 2, 272 / 2);

	checkpointNext("Modes:");
	testSetMode("  Mode -1", -1, 480, 272);
	testSetMode("  Mode 1", 1, 480, 272);
	testSetMode("  Mode 2", 2, 480, 272);
	testSetMode("  Mode 3", 2, 480, 272);
	testSetMode("  Component 720x480", 1, 720, 480);
	testSetMode("  Composite 720x503", 1, 720, 503);

	checkpointNext("Wait behavior:");
	while (sceDisplayIsVblank()) {
		continue;
	}
	checkpoint("  Vblank before set: %d", sceDisplayIsVblank());
	testSetMode("  Set mode", 0, 480, 272);
	checkpoint("  Vblank after set: %d", sceDisplayIsVblank());

	return 0;
}