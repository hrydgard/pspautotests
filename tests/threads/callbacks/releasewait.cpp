#include "shared.h"

// sceKernelReleaseWaitThread() and callbacks: called from inside one, and on a thread that's
// running one.

static SceUID other;
static SceUID sleeper;
static SceUID sleeperCb;

static int delayFunc(SceSize argc, void *argv) {
	schedf("  * other: delay returned %08x\n", sceKernelDelayThread(100000));
	return 0;
}

// arg1 is the notify count, arg2 what sceKernelNotifyCallback passed.
static int mainCallback(int arg1, int arg2, void *arg) {
	schedf("  * main callback: %08x\n", arg2);
	if (arg2 == 1) {
		schedf("  main callback: release other: %08x\n", sceKernelReleaseWaitThread(other));
	} else if (arg2 == 2) {
		schedf("  main callback: release self: %08x\n", sceKernelReleaseWaitThread(sceKernelGetThreadId()));
	}
	schedf("  main callback returning\n");
	return 0;
}

static int sleeperCallback(int arg1, int arg2, void *arg) {
	schedf("  * sleeper callback: %08x\n", arg2);
	if (arg2 == 1) {
		schedf("  sleeper callback: release self: %08x\n", sceKernelReleaseWaitThread(sceKernelGetThreadId()));
	} else if (arg2 == 2) {
		schedf("  sleeper callback: delay returned %08x\n", sceKernelDelayThread(10000));
	}
	schedf("  sleeper callback returning\n");
	return 0;
}

static int sleeperFunc(SceSize argc, void *argv) {
	sleeperCb = sceKernelCreateCallback("sleeper", &sleeperCallback, NULL);
	schedf("  * sleeper: sleep returned %08x\n", sceKernelSleepThreadCB());
	sceKernelDeleteCallback(sleeperCb);
	return 0;
}

static void startSleeper() {
	sleeper = sceKernelCreateThread("sleeper", &sleeperFunc, 0x18, 0x1000, 0, NULL);
	sceKernelStartThread(sleeper, 0, NULL);
}

static void endSleeper() {
	// If it's still asleep, this ends it.
	sceKernelWakeupThread(sleeper);
	SceUInt timeout = 100000;
	sceKernelWaitThreadEnd(sleeper, &timeout);
	sceKernelDeleteThread(sleeper);
	schedf("  sleeper done\n");
}

extern "C" int main(int argc, char *argv[]) {
	SceUID mainCb = sceKernelCreateCallback("main", &mainCallback, NULL);

	schedf("Releasing another thread from a callback:\n");
	other = sceKernelCreateThread("other", &delayFunc, 0x18, 0x1000, 0, NULL);
	sceKernelStartThread(other, 0, NULL);
	sceKernelNotifyCallback(mainCb, 1);
	schedf("  check callbacks: %d\n", sceKernelCheckCallback());
	SceUInt timeout = 100000;
	sceKernelWaitThreadEnd(other, &timeout);
	sceKernelDeleteThread(other);

	schedf("\nReleasing the thread the callback runs on:\n");
	sceKernelNotifyCallback(mainCb, 2);
	schedf("  check callbacks: %d\n", sceKernelCheckCallback());

	schedf("\nFrom its own callback, on a thread in sceKernelSleepThreadCB:\n");
	startSleeper();
	sceKernelNotifyCallback(sleeperCb, 1);
	schedf("  main: notified\n");
	endSleeper();

	schedf("\nA thread whose callback is waiting:\n");
	startSleeper();
	sceKernelNotifyCallback(sleeperCb, 2);
	schedf("  main: release sleeper: %08x\n", sceKernelReleaseWaitThread(sleeper));
	endSleeper();

	schedf("\nA thread in sceKernelSleepThreadCB, no callback:\n");
	startSleeper();
	schedf("  main: release sleeper: %08x\n", sceKernelReleaseWaitThread(sleeper));
	endSleeper();

	sceKernelDeleteCallback(mainCb);
	flushschedf();
	return 0;
}
