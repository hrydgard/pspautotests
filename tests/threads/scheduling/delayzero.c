#include <common.h>
#include <pspthreadman.h>

// Does sceKernelDelayThread(0) give up the CPU? Each call is timed alone, then with a thread that
// spins at a worse priority, counting how often the spinner ran. Then how a new thread's first
// delay goes: a short one can return at once there, when a later one wouldn't.

static volatile int stopSpin = 0;
static volatile u32 spins = 0;

static int spinThread(SceSize argc, void *argp) {
	while (!stopSpin) {
		spins++;
	}
	return 0;
}

// Coarse, since the counts vary from run to run.
static const char *share(int n, int total) {
	if (n * 10 < total) {
		return "almost never";
	} else if (n * 10 < total * 9) {
		return "sometimes";
	}
	return "almost always";
}

static void run(const char *title, u32 delay, int spinPrio) {
	SceUID spinner = -1;
	if (spinPrio) {
		stopSpin = 0;
		spinner = sceKernelCreateThread("spin", &spinThread, spinPrio, 0x1000, 0, NULL);
		sceKernelStartThread(spinner, 0, NULL);
	}

	// How many returned at once, and how many times the spinner got in.
	int shortCount = 0, ran = 0;
	for (int i = 0; i < 32; ++i) {
		u32 before = spins;
		u32 start = sceKernelGetSystemTimeLow();
		sceKernelDelayThread(delay);
		u32 t = sceKernelGetSystemTimeLow() - start;
		if (spins != before) {
			ran++;
		}
		if (t < 100) {
			shortCount++;
		}
	}

	if (spinner >= 0) {
		stopSpin = 1;
		sceKernelTerminateDeleteThread(spinner);
	}
	schedf("  %s: returns at once %s, spinner runs %s\n", title, share(shortCount, 32), share(ran, 32));
}

static volatile u32 firstTook;

static int firstDelayThread(SceSize argc, void *argp) {
	u32 start = sceKernelGetSystemTimeLow();
	sceKernelDelayThread(*(int *)argp);
	firstTook = sceKernelGetSystemTimeLow() - start;
	return 0;
}

static void runFirst(int delay) {
	int shortCount = 0;
	for (int i = 0; i < 16; ++i) {
		SceUID thread = sceKernelCreateThread("first", &firstDelayThread, 0x18, 0x10000, 0, NULL);
		sceKernelStartThread(thread, sizeof(delay), &delay);
		sceKernelWaitThreadEnd(thread, NULL);
		sceKernelDeleteThread(thread);
		if (firstTook < 100) {
			shortCount++;
		}
	}
	schedf("  %d, a new thread's first delay: returns at once %s\n", delay, share(shortCount, 16));
}

int main(int argc, char *argv[]) {
	checkpointNext("sceKernelDelayThread:");
	int prio = sceKernelGetThreadCurrentPriority();
	run("0, alone", 0, 0);
	run("0, worse thread spinning", 0, prio + 1);
	run("1, alone", 1, 0);
	run("1, worse thread spinning", 1, prio + 1);
	run("100, worse thread spinning", 100, prio + 1);
	runFirst(0);
	runFirst(1);
	runFirst(2);
	runFirst(5);
	flushschedf();
	return 0;
}
