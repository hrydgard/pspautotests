#include <common.h>
#include <pspthreadman.h>

// Does a better thread whose wait ends get the CPU from a thread that is running plain user code,
// making no syscalls? A thread at 0x10 waits 1000us eight times and measures how long each wait
// really took, while main (0x20) either blocks or spins without calling anything.

static volatile int done = 0;
static u32 longest = 0, shortest = 0xFFFFFFFF;
static SceUID self;

static void record(u32 t) {
	longest = t > longest ? t : longest;
	shortest = t < shortest ? t : shortest;
}

static int delayThread(SceSize argc, void *argp) {
	for (int i = 0; i < 8; ++i) {
		u32 start = sceKernelGetSystemTimeLow();
		sceKernelDelayThread(1000);
		record(sceKernelGetSystemTimeLow() - start);
	}
	done = 1;
	return 0;
}

static volatile u32 handlerAt;
static u32 handlerLongest = 0, handlerShortest = 0xFFFFFFFF;

static SceUInt alarmHandler(void *common) {
	handlerAt = sceKernelGetSystemTimeLow();
	sceKernelWakeupThread(self);
	return 0;
}

static int alarmThread(SceSize argc, void *argp) {
	self = sceKernelGetThreadId();
	for (int i = 0; i < 8; ++i) {
		u32 start = sceKernelGetSystemTimeLow();
		sceKernelSetAlarm(1000, &alarmHandler, NULL);
		sceKernelSleepThread();
		u32 now = sceKernelGetSystemTimeLow();
		record(now - start);
		u32 h = handlerAt - start;
		handlerLongest = h > handlerLongest ? h : handlerLongest;
		handlerShortest = h < handlerShortest ? h : handlerShortest;
	}
	done = 1;
	return 0;
}

static const char *bucket(u32 us) {
	static char buf[4][16];
	static int n = 0;
	char *b = buf[n++ & 3];
	sprintf(b, "~%dus", (int)((us + 5) / 10 * 10));
	return b;
}

static void run(const char *title, SceKernelThreadEntry entry, int spin) {
	done = 0;
	longest = 0;
	shortest = 0xFFFFFFFF;
	SceUID t = sceKernelCreateThread("better", entry, 0x10, 0x1000, 0, NULL);
	sceKernelStartThread(t, 0, NULL);
	if (spin) {
		volatile u32 spins = 0;
		while (!done) {
			spins++;
		}
	}
	sceKernelWaitThreadEnd(t, NULL);
	sceKernelDeleteThread(t);
	schedf("  %s: %s to %s\n", title, bucket(shortest), bucket(longest));
	if (entry == &alarmThread) {
		schedf("    handler ran after %s to %s\n", bucket(handlerShortest), bucket(handlerLongest));
		handlerLongest = 0;
		handlerShortest = 0xFFFFFFFF;
	}
}

int main(int argc, char *argv[]) {
	checkpointNext("1000us waits, shortest to longest:");
	run("Delay, main blocked", &delayThread, 0);
	run("Delay, main spinning", &delayThread, 1);
	run("Alarm, main blocked", &alarmThread, 0);
	run("Alarm, main spinning", &alarmThread, 1);
	flushschedf();
	return 0;
}
