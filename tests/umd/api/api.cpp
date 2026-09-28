#include <common.h>
#include <pspthreadman.h>
#include <pspumd.h>

// sceUmdUser behaviour that doesn't depend on whether a disc is inserted. (Recorded without one.)
// In the firmware (mediaman.prx) the drive state is an event flag, and every wait is a wait on it.

extern "C" int sceKernelCancelSema(SceUID semaid, int count, int *numWaitThreads);

static SceUID cb;
static volatile int cancelAt = 0;

static int cbFunc(int arg1, int arg2, void *arg) {
	schedf("  * callback: %08x, %08x\n", arg1, arg2);
	return 0;
}

static int cancelerFunc(SceSize argc, void *argv) {
	sceKernelDelayThread(cancelAt);
	schedf("  canceler: sceUmdCancelWaitDriveStat: %08x\n", sceUmdCancelWaitDriveStat());
	return sceKernelExitDeleteThread(0);
}

static void startCanceler(int at) {
	cancelAt = at;
	SceUID thread = sceKernelCreateThread("canceler", &cancelerFunc, 0x10, 0x1000, 0, NULL);
	sceKernelStartThread(thread, 0, NULL);
}

static const char *elapsedBucket(u32 us) {
	if (us < 100) {
		return "<100us";
	} else if (us < 1000) {
		return "<1ms";
	} else if (us < 4000) {
		return "1-4ms";
	} else if (us < 6000) {
		return "4-6ms";
	}
	return ">=6ms";
}

static const char *timeoutBucket(u32 us) {
	if (us < 150) {
		return "<150us";
	} else if (us < 350) {
		return "~250us";
	} else if (us < 700) {
		return "~400us";
	} else if (us < 1500) {
		return "~1ms";
	}
	return "longer";
}

static void testRegister() {
	checkpointNext("sceUmdRegisterUMDCallBack types:");
	SceUID sema = sceKernelCreateSema("sema", 0, 0, 1, NULL);
	SceUID flag = sceKernelCreateEventFlag("flag", 0, 0, NULL);
	checkpoint("  Semaphore: %08x", sceUmdRegisterUMDCallBack(sema));
	checkpoint("  Event flag: %08x", sceUmdRegisterUMDCallBack(flag));
	checkpoint("  Thread: %08x", sceUmdRegisterUMDCallBack(sceKernelGetThreadId()));
	checkpoint("  Callback: %08x", sceUmdRegisterUMDCallBack(cb));
	checkpoint("  Unregister semaphore: %08x", sceUmdUnRegisterUMDCallBack(sema));
	checkpoint("  Unregister callback: %08x", sceUmdUnRegisterUMDCallBack(cb) == cb ? 0 : -1);
	sceKernelDeleteEventFlag(flag);
	sceKernelDeleteSema(sema);
}

static void testActivateParams() {
	checkpointNext("sceUmdActivate params:");
	static const char *names[] = { "disc0:", "disc0", "disc0:/", "DISC0:", "umd0:", "", NULL };
	static const char *titles[] = { "disc0:", "disc0", "disc0:/", "DISC0:", "umd0:", "empty", "NULL" };
	for (int i = 0; i < (int)ARRAY_SIZE(names); ++i) {
		checkpoint("  Mode 1, %s: %08x", titles[i], sceUmdActivate(1, names[i]));
	}
	checkpoint("  Mode 0: %08x", sceUmdActivate(0, "disc0:"));
	checkpoint("  Mode 2: %08x", sceUmdActivate(2, "disc0:"));
	checkpoint("  Mode 3: %08x", sceUmdActivate(3, "disc0:"));
	checkpoint("  Mode -1: %08x", sceUmdActivate(-1, "disc0:"));
	checkpoint("  Kernel pointer: %08x", sceUmdActivate(1, (const char *)0x88000000));
}

static void testDeactivateParams() {
	checkpointNext("sceUmdDeactivate params:");
	checkpoint("  Mode 19: %08x", sceUmdDeactivate(19, "disc0:"));
	checkpoint("  Mode -1: %08x", sceUmdDeactivate(-1, "disc0:"));
	checkpoint("  Mode 2, NULL: %08x", sceUmdDeactivate(2, NULL));
	checkpoint("  Kernel pointer: %08x", sceUmdDeactivate(1, (const char *)0x88000000));
	// Put it back.
	sceUmdActivate(1, "disc0:");
}

static void testWaitParams() {
	checkpointNext("Wait stat validation:");
	static const u32 stats[] = { 0, 0x04, 0x40, 0x80, 0xFFFFFFC4, 0x01, 0x02, 0x08, 0x10, 0x20 };
	for (int i = 0; i < (int)ARRAY_SIZE(stats); ++i) {
		// Only the invalid ones reach an error without waiting; the valid ones time out quickly or match.
		int r1 = sceUmdWaitDriveStatWithTimer(stats[i], 100);
		int r2 = sceUmdWaitDriveStatCB(stats[i], 100);
		// No checkpoint(): whether these waited at all depends on the disc.
		schedf("  %08x: WithTimer %s, CB %s\n", stats[i],
			r1 == 0 || r1 == (int)0x800201A8 ? "ok" : "error", r2 == 0 || r2 == (int)0x800201A8 ? "ok" : "error");
		if (r1 != 0 && r1 != (int)0x800201A8) {
			schedf("    WithTimer: %08x, CB: %08x\n", r1, r2);
		}
	}
	flushschedf();
}

// Needs a stat that never becomes true during the test. NOT_READY is only transient.
static const u32 NEVER = 0x08;

static void testZeroTimeout() {
	checkpointNext("Zero timeout:");
	startCanceler(5000);
	u32 start = sceKernelGetSystemTimeLow();
	int result = sceUmdWaitDriveStatWithTimer(NEVER, 0);
	schedf("  sceUmdWaitDriveStatWithTimer(0): %08x after %s\n", result, elapsedBucket(sceKernelGetSystemTimeLow() - start));
	flushschedf();

	startCanceler(5000);
	start = sceKernelGetSystemTimeLow();
	result = sceUmdWaitDriveStatCB(NEVER, 0);
	schedf("  sceUmdWaitDriveStatCB(0): %08x after %s\n", result, elapsedBucket(sceKernelGetSystemTimeLow() - start));
	flushschedf();

	startCanceler(5000);
	start = sceKernelGetSystemTimeLow();
	result = sceUmdWaitDriveStat(NEVER);
	schedf("  sceUmdWaitDriveStat: %08x after %s\n", result, elapsedBucket(sceKernelGetSystemTimeLow() - start));
	flushschedf();
}

static void testTimeouts() {
	checkpointNext("Timeouts:");
	// 1 and 2 are left out: on hardware they come out short or not, varying from run to run.
	static const u32 timeouts[] = { 5, 20, 100, 200, 400, 1000 };
	for (int i = 0; i < (int)ARRAY_SIZE(timeouts); ++i) {
		// The shortest of a few, to keep interruptions out of it.
		u32 t1 = 0xFFFFFFFF, t2 = 0xFFFFFFFF;
		int r1 = 0, r2 = 0;
		for (int j = 0; j < 3; ++j) {
			u32 start = sceKernelGetSystemTimeLow();
			r1 = sceUmdWaitDriveStatWithTimer(NEVER, timeouts[i]);
			u32 t = sceKernelGetSystemTimeLow() - start;
			t1 = t < t1 ? t : t1;
			start = sceKernelGetSystemTimeLow();
			r2 = sceUmdWaitDriveStatCB(NEVER, timeouts[i]);
			t = sceKernelGetSystemTimeLow() - start;
			t2 = t < t2 ? t : t2;
		}
		schedf("  %4d: WithTimer %08x %s, CB %08x %s\n", timeouts[i], r1, timeoutBucket(t1), r2, timeoutBucket(t2));
	}
	flushschedf();
}

static void testCancel() {
	checkpointNext("sceUmdCancelWaitDriveStat:");
	checkpoint("  No waiters: %08x", sceUmdCancelWaitDriveStat());
	startCanceler(1000);
	int result = sceUmdWaitDriveStatWithTimer(NEVER, 5000);
	schedf("  WithTimer: %08x\n", result);
	flushschedf();

	// A cancel while the wait is paused for a callback.
	checkpointNext("Cancel during a callback:");
	sceKernelNotifyCallback(cb, 0x77);
	startCanceler(1000);
	result = sceUmdWaitDriveStatCB(NEVER, 3000);
	schedf("  CB: %08x\n", result);
	flushschedf();
}

static int cancelInCallback(int arg1, int arg2, void *arg) {
	schedf("  * callback canceling: %08x\n", sceUmdCancelWaitDriveStat());
	return 0;
}

static void testCancelInCallback() {
	checkpointNext("Cancel from the callback itself:");
	SceUID cb2 = sceKernelCreateCallback("cancel", &cancelInCallback, NULL);
	sceKernelNotifyCallback(cb2, 1);
	int result = sceUmdWaitDriveStatCB(NEVER, 3000);
	schedf("  CB: %08x\n", result);
	flushschedf();
	sceKernelDeleteCallback(cb2);
}

static void testSatisfied() {
	checkpointNext("Already satisfied:");
	u32 stat = sceUmdGetDriveStat();
	checkpoint("  sceUmdWaitDriveStat: %08x", sceUmdWaitDriveStat(stat));
	checkpoint("  sceUmdWaitDriveStatWithTimer: %08x", sceUmdWaitDriveStatWithTimer(stat, 1000));
	checkpoint("  sceUmdWaitDriveStatCB: %08x", sceUmdWaitDriveStatCB(stat, 1000));
	sceKernelNotifyCallback(cb, 0x55);
	checkpoint("  sceUmdWaitDriveStatCB with callback: %08x", sceUmdWaitDriveStatCB(stat, 1000));
	sceKernelNotifyCallback(cb, 0x56);
	checkpoint("  sceUmdWaitDriveStat with callback: %08x", sceUmdWaitDriveStat(stat));
	checkpoint("  sceKernelCheckCallback: %08x", sceKernelCheckCallback());
}

static void testDispatch() {
	checkpointNext("Dispatch disabled:");
	u32 stat = sceUmdGetDriveStat();
	int state = sceKernelSuspendDispatchThread();
	int r1 = sceUmdWaitDriveStat(stat);
	int r2 = sceUmdWaitDriveStat(NEVER);
	int r3 = sceUmdWaitDriveStat(0);
	int r4 = sceUmdWaitDriveStatWithTimer(NEVER, 100);
	int r5 = sceUmdWaitDriveStatCB(NEVER, 100);
	sceKernelResumeDispatchThread(state);
	checkpoint("  Satisfied: %08x", r1);
	checkpoint("  Not satisfied: %08x", r2);
	checkpoint("  Invalid: %08x", r3);
	checkpoint("  WithTimer: %08x", r4);
	checkpoint("  CB: %08x", r5);
}

static void testDiscInfo() {
	checkpointNext("sceUmdGetDiscInfo params:");
	pspUmdInfo info;
	checkpoint("  NULL: %08x", sceUmdGetDiscInfo(NULL));
	info.size = 0;
	checkpoint("  Size 0: %08x", sceUmdGetDiscInfo(&info));
	info.size = 12;
	checkpoint("  Size 12: %08x", sceUmdGetDiscInfo(&info));
	checkpoint("  Kernel pointer: %08x", sceUmdGetDiscInfo((pspUmdInfo *)0x88000000));
}

extern "C" int main(int argc, char *argv[]) {
	cb = sceKernelCreateCallback("umd", &cbFunc, NULL);

	testRegister();
	testActivateParams();
	testDeactivateParams();
	testWaitParams();
	testZeroTimeout();
	testTimeouts();
	testCancel();
	testCancelInCallback();
	testSatisfied();
	testDispatch();
	testDiscInfo();

	sceKernelDeleteCallback(cb);
	return 0;
}
