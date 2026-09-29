#include <common.h>
#include <pspthreadman.h>
#include <pspctrl.h>
#include <pspdisplay.h>

// How long sceCtrlPeekBuffer* take by the number of samples asked for. On hardware ~2us, plus ~0.2us
// for each sample copied: 64 take ~15us.

static SceCtrlData pad[64];

static int timeIt(int (*fn)(SceCtrlData *, int), int count) {
	int best = 100000;
	for (int i = 0; i < 20; ++i) {
		u32 t0 = sceKernelGetSystemTimeLow();
		fn(pad, count);
		u32 t1 = sceKernelGetSystemTimeLow();
		if ((int)(t1 - t0) < best) {
			best = t1 - t0;
		}
	}
	return best;
}

static const char *bucket(int us) {
	if (us <= 3) {
		return "0-3us";
	} else if (us <= 6) {
		return "4-6us";
	} else if (us <= 10) {
		return "7-10us";
	} else if (us <= 20) {
		return "11-20us";
	}
	return "over 20us";
}

extern "C" int main(int argc, char *argv[]) {
	sceCtrlSetSamplingCycle(0);
	// Let the buffer fill, so every count gets as many samples as it asks for.
	for (int i = 0; i < 70; ++i) {
		sceDisplayWaitVblankStart();
	}

	checkpointNext("Peek:");
	static const int counts[] = { 1, 16, 32, 64 };
	for (int c : counts) {
		checkpoint("  %2d samples: positive %s, negative %s", c, bucket(timeIt(sceCtrlPeekBufferPositive, c)), bucket(timeIt(sceCtrlPeekBufferNegative, c)));
	}
	return 0;
}
