#include <common.h>
#include <pspthreadman.h>

// How long it takes the CPU to get from one thread to another at each kind of handoff, to 5us,
// the shortest of several tries. Main runs at 0x20, the other thread at 0x10 (better) unless noted.

static volatile u32 stamp;
static SceUID semaA, semaB;

static int enterExitThread(SceSize argc, void *argp) {
	u32 now = sceKernelGetSystemTimeLow();
	*(u32 *)(*(u32 *)argp) = now;
	stamp = sceKernelGetSystemTimeLow();
	return 0;
}

static int exitThreadThread(SceSize argc, void *argp) {
	u32 now = sceKernelGetSystemTimeLow();
	*(u32 *)(*(u32 *)argp) = now;
	stamp = sceKernelGetSystemTimeLow();
	sceKernelExitThread(0);
	return 0;
}

static int blockThread(SceSize argc, void *argp) {
	// Waits on A; when woken, stamps and blocks on B, handing the CPU back.
	for (int i = 0; i < 8; ++i) {
		sceKernelWaitSema(semaA, 1, NULL);
		stamp = sceKernelGetSystemTimeLow();
		sceKernelWaitSema(semaB, 1, NULL);
	}
	return 0;
}

static int delayThread(SceSize argc, void *argp) {
	for (int i = 0; i < 8; ++i) {
		u32 start = sceKernelGetSystemTimeLow();
		sceKernelDelayThread(1000);
		((u32 *)(*(u32 *)argp))[i] = sceKernelGetSystemTimeLow() - start;
	}
	return 0;
}

static const char *bucket(u32 us) {
	static char buf[4][16];
	static int n = 0;
	char *b = buf[n++ & 3];
	sprintf(b, "~%dus", (us + 2) / 5 * 5);
	return b;
}

static void measureStartExit(const char *title, SceKernelThreadEntry entry) {
	u32 bestIn = 0xFFFFFFFF, bestOut = 0xFFFFFFFF;
	for (int i = 0; i < 8; ++i) {
		u32 entered = 0;
		u32 addr = (u32)&entered;
		SceUID thread = sceKernelCreateThread("handoff", entry, 0x10, 0x1000, 0, NULL);
		u32 start = sceKernelGetSystemTimeLow();
		sceKernelStartThread(thread, sizeof(addr), &addr);
		u32 back = sceKernelGetSystemTimeLow();
		sceKernelDeleteThread(thread);
		bestIn = entered - start < bestIn ? entered - start : bestIn;
		bestOut = back - stamp < bestOut ? back - stamp : bestOut;
	}
	schedf("  %s: start to entry %s, end to back in main %s\n", title, bucket(bestIn), bucket(bestOut));
}

int main(int argc, char *argv[]) {
	checkpointNext("Handoffs:");
	measureStartExit("Returning", &enterExitThread);
	measureStartExit("sceKernelExitThread", &exitThreadThread);

	// Main signals A: the better thread wakes at once. It stamps and blocks on B: main is back.
	semaA = sceKernelCreateSema("a", 0, 0, 1, NULL);
	semaB = sceKernelCreateSema("b", 0, 0, 1, NULL);
	SceUID thread = sceKernelCreateThread("block", &blockThread, 0x10, 0x1000, 0, NULL);
	sceKernelStartThread(thread, 0, NULL);
	u32 bestWake = 0xFFFFFFFF, bestBlock = 0xFFFFFFFF;
	for (int i = 0; i < 8; ++i) {
		u32 start = sceKernelGetSystemTimeLow();
		sceKernelSignalSema(semaA, 1);
		u32 back = sceKernelGetSystemTimeLow();
		bestWake = stamp - start < bestWake ? stamp - start : bestWake;
		bestBlock = back - stamp < bestBlock ? back - stamp : bestBlock;
		sceKernelSignalSema(semaB, 1);
	}
	sceKernelWaitThreadEnd(thread, NULL);
	sceKernelDeleteThread(thread);
	schedf("  sceKernelSignalSema waking a better thread: %s, it blocking again to back in main %s\n", bucket(bestWake), bucket(bestBlock));

	// A delay, measured from inside.
	u32 delays[8];
	u32 addr = (u32)delays;
	thread = sceKernelCreateThread("delay", &delayThread, 0x10, 0x1000, 0, NULL);
	sceKernelStartThread(thread, sizeof(addr), &addr);
	sceKernelWaitThreadEnd(thread, NULL);
	sceKernelDeleteThread(thread);
	u32 bestDelay = 0xFFFFFFFF;
	for (int i = 0; i < 8; ++i) {
		bestDelay = delays[i] < bestDelay ? delays[i] : bestDelay;
	}
	schedf("  sceKernelDelayThread(1000): %s\n", bucket(bestDelay));

	flushschedf();
	return 0;
}
