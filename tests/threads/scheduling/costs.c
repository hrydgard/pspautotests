#include <common.h>
#include <pspthreadman.h>

// How long thread management calls take, in coarse buckets. Tests like scheduling depend on it:
// a thread delaying 500us gets to run a few times while main creates and starts others.

static int lowThread(SceSize argc, void *argp) {
	return 0;
}

static int delayThread(SceSize argc, void *argp) {
	sceKernelDelayThread(*(int *)argp);
	return 0;
}

static const char *bucket(u32 us) {
	static char buf[32];
	if (us < 10) {
		return "<10us";
	} else if (us < 25) {
		return "10-25us";
	} else if (us < 50) {
		return "25-50us";
	} else if (us < 100) {
		return "50-100us";
	}
	// Above that, to the nearest 50us.
	sprintf(buf, "~%dus", (us + 25) / 50 * 50);
	return buf;
}

// Shortest of a few runs, to keep interrupts out of it.
#define MEASURE(result, expr) { \
	u32 best = 0xFFFFFFFF; \
	for (int i = 0; i < 5; ++i) { \
		u32 start = sceKernelGetSystemTimeLow(); \
		expr; \
		u32 t = sceKernelGetSystemTimeLow() - start; \
		best = t < best ? t : best; \
	} \
	result = best; \
}

static void testCreate(int stackSize) {
	u32 createTime = 0xFFFFFFFF, startTime = 0xFFFFFFFF, deleteTime = 0xFFFFFFFF;
	for (int i = 0; i < 5; ++i) {
		u32 start = sceKernelGetSystemTimeLow();
		SceUID thread = sceKernelCreateThread("cost", &lowThread, 0x30, stackSize, 0, NULL);
		u32 t1 = sceKernelGetSystemTimeLow();
		sceKernelStartThread(thread, 0, NULL);
		u32 t2 = sceKernelGetSystemTimeLow();
		// Let it run and finish.
		sceKernelDelayThread(1000);
		u32 t3 = sceKernelGetSystemTimeLow();
		sceKernelDeleteThread(thread);
		u32 t4 = sceKernelGetSystemTimeLow();
		createTime = t1 - start < createTime ? t1 - start : createTime;
		startTime = t2 - t1 < startTime ? t2 - t1 : startTime;
		deleteTime = t4 - t3 < deleteTime ? t4 - t3 : deleteTime;
	}
	schedf("  Stack 0x%05x: create %s, ", stackSize, bucket(createTime));
	schedf("start %s, ", bucket(startTime));
	schedf("delete %s\n", bucket(deleteTime));
}

static void testDelayLatency(int delay) {
	// A better priority thread delays; main spins until it's done and sees how long that took.
	u32 best = 0xFFFFFFFF;
	for (int i = 0; i < 5; ++i) {
		SceUID thread = sceKernelCreateThread("delay", &delayThread, 0x10, 0x1000, 0, NULL);
		u32 start = sceKernelGetSystemTimeLow();
		sceKernelStartThread(thread, sizeof(delay), &delay);
		sceKernelWaitThreadEnd(thread, NULL);
		u32 t = sceKernelGetSystemTimeLow() - start;
		best = t < best ? t : best;
		sceKernelDeleteThread(thread);
	}
	schedf("  Start, sceKernelDelayThread(%d), end: %s\n", delay, bucket(best));
}

int main(int argc, char *argv[]) {
	u32 t;

	checkpointNext("Thread lifetime:");
	testCreate(0x1000);
	testCreate(0x10000);
	testCreate(0x40000);
	flushschedf();

	checkpointNext("Delays:");
	testDelayLatency(1);
	testDelayLatency(100);
	testDelayLatency(500);
	flushschedf();

	checkpointNext("Misc:");
	MEASURE(t, sceKernelGetThreadId());
	schedf("  sceKernelGetThreadId: %s\n", bucket(t));
	MEASURE(t, sceKernelDelayThread(0));
	schedf("  sceKernelDelayThread(0): %s\n", bucket(t));
	SceUID sema = sceKernelCreateSema("sema", 0, 0, 1, NULL);
	MEASURE(t, sceKernelSignalSema(sema, 1); sceKernelWaitSema(sema, 1, NULL));
	schedf("  sceKernelSignalSema + sceKernelWaitSema: %s\n", bucket(t));
	sceKernelDeleteSema(sema);
	MEASURE(t, checkpoint(NULL));
	schedf("  checkpoint(NULL): %s\n", bucket(t));
	flushschedf();
	return 0;
}
