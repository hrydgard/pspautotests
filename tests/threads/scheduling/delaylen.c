#include <common.h>
#include <pspthreadman.h>

// How long sceKernelDelayThread(n) actually takes, from inside the delaying thread, with no other
// thread wanting the CPU. To 10us, the shortest of several tries.

static const u32 delays[] = { 0, 1, 2, 5, 10, 20, 50, 100, 150, 190, 200, 205, 209, 210, 215, 220, 250, 300, 500, 1000 };

static u32 measure(u32 delay, int cb) {
	u32 best = 0xFFFFFFFF;
	for (int i = 0; i < 8; ++i) {
		u32 start = sceKernelGetSystemTimeLow();
		if (cb) {
			sceKernelDelayThreadCB(delay);
		} else {
			sceKernelDelayThread(delay);
		}
		u32 t = sceKernelGetSystemTimeLow() - start;
		best = t < best ? t : best;
	}
	return best;
}

int main(int argc, char *argv[]) {
	checkpointNext("sceKernelDelayThread:");
	for (int i = 0; i < (int)ARRAY_SIZE(delays); ++i) {
		u32 t = measure(delays[i], 0);
		u32 tcb = measure(delays[i], 1);
		schedf("  %4d: ~%dus, CB ~%dus\n", delays[i], (t + 5) / 10 * 10, (tcb + 5) / 10 * 10);
	}
	flushschedf();
	return 0;
}
