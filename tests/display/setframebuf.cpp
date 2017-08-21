#include <common.h>
#include <pspdisplay.h>

extern "C" int HAS_DISPLAY;

static void *vbuf1 = (void *)0x04000000;
static void *vbuf2 = (void *)0x04088000;
static void *vbufMisaligned = (void *)0x04088004;
static void *vbufAligned16 = (void *)0x04088010;
static void *vbufUncached = (void *)0x44088000;
static void *vbufRam = (void *)0x08800000;
static void *vbufScratchpad = (void *)0x00100000;
static void *vbufInvalid = (void *)0x06800000;

void schedfFrameBufs(const char *title) {
	void *newAddr;
	int newBufw, newFmt;
	sceDisplayGetFrameBuf(&newAddr, &newBufw, &newFmt, 1);
	checkpoint("%s (latched): addr=%08x, bufw=%d, fmt=%d", title, newAddr, newBufw, newFmt);

	sceDisplayGetFrameBuf(&newAddr, &newBufw, &newFmt, 0);
	checkpoint("%s (immediate): addr=%08x, bufw=%d, fmt=%d", title, newAddr, newBufw, newFmt);
}

void testSetFrameBuf(const char *title, void *addr, int bufw, int fmt, int sync) {
	int result = sceDisplaySetFrameBuf(addr, bufw, fmt, sync);
	void *newAddr;
	int newBufw, newFmt;
	sceDisplayGetFrameBuf(&newAddr, &newBufw, &newFmt, sync);

	if (result < 0) {
		checkpoint("%s: Failed (%08x), addr=%08x, bufw=%d, fmt=%d", title, result, newAddr, newBufw, newFmt);
	} else {
		checkpoint("%s: OK (%08x), addr=%08x, bufw=%d, fmt=%d", title, result, newAddr, newBufw, newFmt);
	}
}

extern "C" int main(int argc, char *argv[]) {
	// Let's not bother debugging to the display, since we'll just mess it up anyway.
	HAS_DISPLAY = 0;

	checkpointNext("Sync modes:");
	testSetFrameBuf("  Immediate", vbuf1, 512, 3, 0);
	testSetFrameBuf("  Latched", vbuf1, 512, 3, 1);
	testSetFrameBuf("  -1", vbuf1, 512, 3, -1);
	testSetFrameBuf("  2", vbuf1, 512, 3, -1);

	checkpointNext("Format:");
	testSetFrameBuf("  Immediate -> 565", vbuf1, 512, 0, 0);
	testSetFrameBuf("  Latched -> 565", vbuf1, 512, 0, 1);
	testSetFrameBuf("  Immediate -> 565 after latch", vbuf1, 512, 0, 0);
	testSetFrameBuf("  Latched -> invalid 4", vbuf1, 512, 4, 1);
	testSetFrameBuf("  Latched -> invalid -1", vbuf1, 512, -1, 1);

	// Reset.
	sceDisplaySetFrameBuf(vbuf1, 512, 3, 1);
	sceDisplaySetFrameBuf(vbuf1, 512, 3, 0);

	checkpointNext("Stride:");
	testSetFrameBuf("  Immediate -> 256", vbuf1, 256, 3, 0);
	testSetFrameBuf("  Latched -> 256", vbuf1, 256, 3, 1);
	testSetFrameBuf("  Immediate -> 256 after latch", vbuf1, 256, 3, 0);

	static const int strides[] = { -64, -1, 0, 1, 16, 32, 64, 96, 128, 448, 480, 512, 768, 1024, 2048, 4096, 32768, 0x01000000 };
	for (size_t i = 0; i < ARRAY_SIZE(strides); ++i) {
		char temp[128];
		snprintf(temp, sizeof(temp), "  Latched -> %d", strides[i]);
		testSetFrameBuf(temp, vbuf1, strides[i], 0, 1);
	}

	// Reset.
	sceDisplaySetFrameBuf(vbuf1, 512, 3, 1);
	sceDisplaySetFrameBuf(vbuf1, 512, 3, 0);

	checkpointNext("Address:");
	testSetFrameBuf("  Immediate -> vbuf2", vbuf2, 512, 3, 0);
	testSetFrameBuf("  Latched -> vbuf2", vbuf2, 512, 3, 1);
	testSetFrameBuf("  Immediate -> vbuf2 after latch", vbuf2, 512, 3, 0);
	testSetFrameBuf("  Immediate -> misaligned", vbufMisaligned, 512, 3, 0);
	testSetFrameBuf("  Immediate -> vbuf1+16", vbufAligned16, 512, 3, 0);
	testSetFrameBuf("  Immediate -> uncached", vbufUncached, 512, 3, 0);
	testSetFrameBuf("  Immediate -> ram", vbufRam, 512, 3, 0);
	testSetFrameBuf("  Immediate -> scratchpad", vbufScratchpad, 512, 3, 0);
	testSetFrameBuf("  Immediate -> invalid", vbufInvalid, 512, 3, 0);

	checkpointNext("Address off:");
	testSetFrameBuf("  Immediate -> OFF", 0, 512, 3, 0);
	testSetFrameBuf("  Latched -> OFF", 0, 512, 3, 1);

	// Reset.
	sceDisplaySetFrameBuf(vbuf1, 512, 3, 1);
	sceDisplaySetFrameBuf(vbuf1, 512, 3, 0);

	testSetFrameBuf("  Immediate -> stride 0 (with format)", 0, 0, 3, 0);
	testSetFrameBuf("  Latched -> stride 0 (with format)", 0, 0, 3, 1);

	// Reset.
	sceDisplaySetFrameBuf(vbuf1, 512, 3, 1);
	sceDisplaySetFrameBuf(vbuf1, 512, 3, 0);

	testSetFrameBuf("  Immediate -> all 0s", 0, 0, 0, 0);
	testSetFrameBuf("  Latched -> all 0s", 0, 0, 0, 1);

	// Reset.
	sceDisplaySetFrameBuf(vbuf1, 512, 3, 1);
	sceDisplaySetFrameBuf(vbuf1, 512, 3, 0);
	sceDisplayWaitVblankStart();

	checkpointNext("Latch behavior (format/stride):");
	schedfFrameBufs("  Before latch");
	testSetFrameBuf("  Latched set with format/stride", vbuf1, 256, 1, 1);
	schedfFrameBufs("  After latch");

	// Reset.
	sceDisplaySetFrameBuf(vbuf1, 512, 3, 1);
	sceDisplaySetFrameBuf(vbuf1, 512, 3, 0);
	sceDisplayWaitVblankStart();

	checkpointNext("Latch behavior (address):");
	schedfFrameBufs("  Before latch");
	testSetFrameBuf("  Latched set with address only", vbuf2, 512, 3, 1);
	schedfFrameBufs("  After latch");
	sceDisplayWaitVblankStart();
	schedfFrameBufs("  After vblank start");

	checkpointNext("While off:");
	sceDisplaySetFrameBuf(0, 512, 3, 1);
	sceDisplaySetFrameBuf(0, 512, 3, 0);
	testSetFrameBuf("  Immediate -> format change", vbuf1, 512, 0, 0);
	testSetFrameBuf("  Immediate -> address only", vbuf1, 512, 3, 0);

	// Observed: address = 0 with any fmt/stride means no display.

	return 0;
}