#include <common.h>
#include <pspdisplay.h>
#include <pspthreadman.h>

// How long vblank lasts, and the hcount at its start and end. Timed from sceDisplayWaitVblankStart
// returning, so the start is a little late; the shortest and longest of many frames are printed
// to 10us.

int sceDisplayIsVblank();
int sceDisplayGetCurrentHcount();

int main(int argc, char *argv[]) {
	u32 minLen = 0xFFFFFFFF, maxLen = 0;
	int minStartH = 1000, maxStartH = 0, minEndH = 1000, maxEndH = 0;
	u32 minFrame = 0xFFFFFFFF, maxFrame = 0;
	u32 lastStart = 0;

	sceDisplayWaitVblankStart();
	for (int i = 0; i < 120; ++i) {
		sceDisplayWaitVblankStart();
		u32 start = sceKernelGetSystemTimeLow();
		int startH = sceDisplayGetCurrentHcount();
		int lastH = startH;
		while (sceDisplayIsVblank()) {
			lastH = sceDisplayGetCurrentHcount();
		}
		u32 len = sceKernelGetSystemTimeLow() - start;
		if (i > 0) {
			u32 frame = start - lastStart;
			minFrame = frame < minFrame ? frame : minFrame;
			maxFrame = frame > maxFrame ? frame : maxFrame;
		}
		lastStart = start;
		minLen = len < minLen ? len : minLen;
		maxLen = len > maxLen ? len : maxLen;
		minStartH = startH < minStartH ? startH : minStartH;
		maxStartH = startH > maxStartH ? startH : maxStartH;
		minEndH = lastH < minEndH ? lastH : minEndH;
		maxEndH = lastH > maxEndH ? lastH : maxEndH;
	}

	checkpointNext("Vblank:");
	schedf("  Length after wait returns: %d-%dus\n", (minLen + 5) / 10 * 10, (maxLen + 5) / 10 * 10);
	schedf("  Hcount at start: %d-%d, last seen in vblank: %d-%d\n", minStartH, maxStartH, minEndH, maxEndH);
	schedf("  Frame: %d-%dus\n", (minFrame + 5) / 10 * 10, (maxFrame + 5) / 10 * 10);
	flushschedf();
	return 0;
}
