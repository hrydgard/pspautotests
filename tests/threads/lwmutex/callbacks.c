#include "shared.h"

// sceKernelLockLwMutexCB with callbacks: pending ones, and the lwmutex being released or deleted
// while a callback interrupts the wait.

enum Action {
	RELEASE,
	DELETE,
	NOTHING,
};

static SceLwMutexWorkarea workarea;
static volatile int workareaAlive = 0;
static volatile enum Action releaserAction;
static volatile int releaserDelay;
static SceUID releaser;

static int releaserFunc(SceSize argc, void *argv) {
	sceKernelLockLwMutex(&workarea, 1, NULL);
	if (releaserDelay != 0) {
		sceKernelDelayThread(releaserDelay);
	} else {
		sceKernelSleepThread();
	}
	switch (releaserAction) {
	case RELEASE:
		sceKernelUnlockLwMutex(&workarea, 1);
		break;
	case DELETE:
		sceKernelDeleteLwMutex(&workarea);
		workareaAlive = 0;
		break;
	case NOTHING:
		break;
	}
	// Keep holding it until told to go.
	sceKernelSleepThread();
	if (releaserAction == NOTHING) {
		sceKernelUnlockLwMutex(&workarea, 1);
	}
	return 0;
}

static int cbFunc(int arg1, int arg2, void *arg) {
	if (arg2 == 1) {
		schedf("    * callback, waking releaser\n");
		sceKernelWakeupThread(releaser);
	} else {
		schedf("    * callback\n");
	}
	return 0;
}

static void runCase(SceUID cb, const char *title, enum Action action, int inCallback, int withTimeout) {
	sceKernelCreateLwMutex(&workarea, "lwmutex", 0, 0, NULL);
	workareaAlive = 1;
	releaserAction = action;
	releaserDelay = inCallback ? 0 : 2000;
	releaser = sceKernelCreateThread("releaser", &releaserFunc, 0x10, 0x1000, 0, NULL);
	sceKernelStartThread(releaser, 0, NULL);

	sceKernelNotifyCallback(cb, inCallback ? 1 : 2);
	SceUInt timeout = 5000;
	int result = sceKernelLockLwMutexCB(&workarea, 1, withTimeout ? &timeout : NULL);
	if (withTimeout) {
		schedf("  %s: %08x (timeout left %s)\n", title, result, timeout == 0 ? "0" : "some");
	} else {
		schedf("  %s: %08x\n", title, result);
	}
	if (result == 0) {
		sceKernelUnlockLwMutex(&workarea, 1);
	}

	sceKernelWakeupThread(releaser);
	sceKernelDelayThread(1000);
	sceKernelTerminateDeleteThread(releaser);
	if (workareaAlive) {
		sceKernelDeleteLwMutex(&workarea);
		workareaAlive = 0;
	}
}

int main(int argc, char *argv[]) {
	SceUID cb = sceKernelCreateCallback("lwmutex", &cbFunc, NULL);

	checkpointNext("Contended:");
	runCase(cb, "released in callback", RELEASE, 1, 0);
	runCase(cb, "deleted in callback", DELETE, 1, 0);
	runCase(cb, "released after callback", RELEASE, 0, 0);
	runCase(cb, "timed out", NOTHING, 1, 1);
	runCase(cb, "released in callback, with timeout", RELEASE, 1, 1);
	flushschedf();

	checkpointNext("Uncontended, with a pending callback:");
	sceKernelCreateLwMutex(&workarea, "lwmutex", 0, 0, NULL);
	sceKernelNotifyCallback(cb, 2);
	int result = sceKernelLockLwMutexCB(&workarea, 1, NULL);
	schedf("  sceKernelLockLwMutexCB: %08x\n", result);
	schedf("  sceKernelGetCallbackCount: %d\n", sceKernelGetCallbackCount(cb));
	sceKernelUnlockLwMutex(&workarea, 1);
	flushschedf();

	checkpointNext("Callback after a contended lock returned:");
	runCase(cb, "released in callback", RELEASE, 1, 0);
	sceKernelNotifyCallback(cb, 2);
	result = sceKernelCheckCallback();
	schedf("  sceKernelCheckCallback: %08x\n", result);
	flushschedf();

	sceKernelDeleteLwMutex(&workarea);
	sceKernelDeleteCallback(cb);
	return 0;
}
