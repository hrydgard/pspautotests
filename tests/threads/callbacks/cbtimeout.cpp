#include "shared.h"

// Does time spent running a callback count against the timeout of the wait it interrupted?
// Each wait has a 10ms timeout, and a callback that busy waits 6ms. A helper thread releases
// the wait at 12ms: if the callback's time counts, the wait has already timed out by then.

extern "C" int sceKernelCancelSema(SceUID semaid, int count, int *numWaitThreads);

static SceUID sema;
static SceUID flag;
static SceUID mainThread;
static SceUID cb;

static void busyWait(u32 us) {
	u32 start = sceKernelGetSystemTimeLow();
	while (sceKernelGetSystemTimeLow() - start < us) {
		continue;
	}
}

static int cbFunc(int arg1, int arg2, void *arg) {
	schedf("  * callback: %08x, %08x\n", arg1, arg2);
	busyWait(6000);
	return 0;
}

struct HelperArgs {
	int notifyAt;
	int releaseAt;
	int what;
};

static int helperFunc(SceSize argc, void *argv) {
	HelperArgs args = *(HelperArgs *)argv;
	int waited = 0;
	if (args.notifyAt > 0) {
		sceKernelDelayThread(args.notifyAt);
		waited = args.notifyAt;
		sceKernelNotifyCallback(cb, 2);
	}
	sceKernelDelayThread(args.releaseAt - waited);
	switch (args.what) {
	case 0: sceKernelSignalSema(sema, 1); break;
	case 1: sceKernelSetEventFlag(flag, 1); break;
	case 2: sceKernelWakeupThread(mainThread); break;
	}
	return sceKernelExitDeleteThread(0);
}

static void startHelper(int notifyAt, int releaseAt, int what) {
	HelperArgs args = { notifyAt, releaseAt, what };
	SceUID thread = sceKernelCreateThread("helper", &helperFunc, 0x10, 0x1000, 0, NULL);
	sceKernelStartThread(thread, sizeof(args), &args);
}

static const char *elapsedBucket(u32 us) {
	if (us < 9000) {
		return "early";
	} else if (us < 11500) {
		return "~10ms";
	} else if (us < 14000) {
		return "~12ms";
	} else if (us < 18000) {
		return "~16ms";
	}
	return "late";
}

static const char *remainingBucket(SceUInt us) {
	if (us == 0) {
		return "0";
	} else if (us < 3000) {
		return "<3ms";
	} else if (us < 6000) {
		return "3-6ms";
	}
	return ">=6ms";
}

static void report(const char *name, int result, u32 start, const SceUInt *timeout) {
	u32 elapsed = sceKernelGetSystemTimeLow() - start;
	if (timeout) {
		schedf("  %s: %08x, elapsed %s, timeout left %s\n", name, result, elapsedBucket(elapsed), remainingBucket(*timeout));
	} else {
		schedf("  %s: %08x, elapsed %s\n", name, result, elapsedBucket(elapsed));
	}
}

static void testSema(int notifyAt) {
	sceKernelCancelSema(sema, 0, NULL);
	if (notifyAt == 0) {
		sceKernelNotifyCallback(cb, 1);
	}
	startHelper(notifyAt, 12000, 0);
	SceUInt timeout = 10000;
	u32 start = sceKernelGetSystemTimeLow();
	int result = sceKernelWaitSemaCB(sema, 1, &timeout);
	report("sceKernelWaitSemaCB", result, start, &timeout);
	sceKernelDelayThread(10000);
}

static void testFlag(int notifyAt) {
	sceKernelClearEventFlag(flag, 0);
	if (notifyAt == 0) {
		sceKernelNotifyCallback(cb, 1);
	}
	startHelper(notifyAt, 12000, 1);
	SceUInt timeout = 10000;
	u32 start = sceKernelGetSystemTimeLow();
	int result = sceKernelWaitEventFlagCB(flag, 1, PSP_EVENT_WAITOR | PSP_EVENT_WAITCLEAR, NULL, &timeout);
	report("sceKernelWaitEventFlagCB", result, start, &timeout);
	sceKernelDelayThread(10000);
}

static void testDelay(int notifyAt) {
	if (notifyAt == 0) {
		sceKernelNotifyCallback(cb, 1);
	} else {
		// Nothing to release, just notify.
		startHelper(notifyAt, notifyAt + 1, 3);
	}
	u32 start = sceKernelGetSystemTimeLow();
	int result = sceKernelDelayThreadCB(10000);
	report("sceKernelDelayThreadCB", result, start, NULL);
	sceKernelDelayThread(10000);
}

extern "C" int main(int argc, char *argv[]) {
	mainThread = sceKernelGetThreadId();
	cb = sceKernelCreateCallback("timeout", &cbFunc, NULL);
	sema = sceKernelCreateSema("sema", 0, 0, 1, NULL);
	flag = sceKernelCreateEventFlag("flag", 0, 0, NULL);

	checkpointNext("Callback pending before the wait:");
	testSema(0);
	testFlag(0);
	testDelay(0);
	flushschedf();

	checkpointNext("Callback notified 3ms into the wait:");
	testSema(3000);
	testFlag(3000);
	testDelay(3000);
	flushschedf();

	checkpointNext("Without a callback:");
	sceKernelCancelSema(sema, 0, NULL);
	startHelper(0, 12000, 0);
	SceUInt timeout = 10000;
	u32 start = sceKernelGetSystemTimeLow();
	int result = sceKernelWaitSemaCB(sema, 1, &timeout);
	report("sceKernelWaitSemaCB", result, start, &timeout);
	sceKernelDelayThread(10000);
	flushschedf();

	sceKernelDeleteEventFlag(flag);
	sceKernelDeleteSema(sema);
	sceKernelDeleteCallback(cb);
	return 0;
}
