#include <common.h>
#include <pspthreadman.h>

// Can a thread preempt a long running syscall? sceKernelCreateThread with a big stack takes over a
// millisecond filling it. Main runs at 0x20; a better thread (0x10) keeps delaying 200us, and a
// worse one (0x30) spins whenever it gets the CPU.

static volatile int highWakes = 0;
static volatile u32 wakeTimes[256];
static volatile int highWorkUs = 0;
static volatile int stopThreads = 0;
static volatile u32 lowSpins = 0;

static void busyWait(u32 us) {
	u32 start = sceKernelGetSystemTimeLow();
	while (sceKernelGetSystemTimeLow() - start < us) {
		continue;
	}
}

static int highThread(SceSize argc, void *argp) {
	while (!stopThreads) {
		sceKernelDelayThread(200);
		wakeTimes[highWakes++ & 255] = sceKernelGetSystemTimeLow();
		if (highWorkUs) {
			busyWait(highWorkUs);
		}
	}
	return 0;
}

static int lowThread(SceSize argc, void *argp) {
	while (!stopThreads) {
		lowSpins++;
	}
	return 0;
}

static int dummyThread(SceSize argc, void *argp) {
	return 0;
}

static int wakesInCreate;

// Shortest of a few, to keep noise out of it. Also counts the better thread's wakes that fell
// inside the call, not just at its end.
static u32 timeCreate(int stackSize) {
	u32 best = 0xFFFFFFFF;
	wakesInCreate = 1000;
	for (int i = 0; i < 3; ++i) {
		int wakesBefore = highWakes;
		u32 start = sceKernelGetSystemTimeLow();
		SceUID thread = sceKernelCreateThread("dummy", &dummyThread, 0x40, stackSize, 0, NULL);
		u32 end = sceKernelGetSystemTimeLow();
		int wakes = 0;
		for (int w = wakesBefore; w < highWakes; ++w) {
			u32 at = wakeTimes[w & 255];
			if (at - start < end - start - 10) {
				wakes++;
			}
		}
		// The fewest in any one call.
		wakesInCreate = wakes < wakesInCreate ? wakes : wakesInCreate;
		sceKernelDeleteThread(thread);
		u32 t = end - start;
		best = t < best ? t : best;
	}
	return best;
}

static const char *countBucket(int n) {
	if (n == 0) {
		return "none";
	} else if (n <= 2) {
		return "1-2";
	}
	return "3 or more";
}

static const char *timeBucket(u32 us) {
	static char buf[32];
	sprintf(buf, "~%dus", (us + 50) / 100 * 100);
	return buf;
}

int main(int argc, char *argv[]) {
	checkpointNext("Alone:");
	schedf("  sceKernelCreateThread(256KB stack): %s\n", timeBucket(timeCreate(0x40000)));
	flushschedf();

	checkpointNext("With a better thread delaying 200us at a time:");
	SceUID high = sceKernelCreateThread("high", &highThread, 0x10, 0x1000, 0, NULL);
	sceKernelStartThread(high, 0, NULL);
		u32 t = timeCreate(0x40000);
	schedf("  sceKernelCreateThread: %s, the better thread ran during it: %s\n", timeBucket(t), countBucket(wakesInCreate));
	flushschedf();

	checkpointNext("With the better thread doing 150us of work each time:");
	highWorkUs = 150;
		t = timeCreate(0x40000);
	schedf("  sceKernelCreateThread: %s, the better thread ran during it: %s\n", timeBucket(t), countBucket(wakesInCreate));
	highWorkUs = 0;
	flushschedf();

	checkpointNext("With a worse thread ready to run:");
	SceUID low = sceKernelCreateThread("low", &lowThread, 0x30, 0x1000, 0, NULL);
	sceKernelStartThread(low, 0, NULL);
	u32 spinsBefore = lowSpins;
	t = timeCreate(0x40000);
	u32 spinsAfter = lowSpins;
	// Stop them first: the worse thread would starve the output going to the host.
	stopThreads = 1;
	sceKernelTerminateDeleteThread(high);
	sceKernelTerminateDeleteThread(low);
	schedf("  sceKernelCreateThread: %s, the worse thread ran during it: %s\n", timeBucket(t), spinsAfter != spinsBefore ? "yes" : "no");
	flushschedf();
	return 0;
}
