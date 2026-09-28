#include "shared.h"
#include <pspumd.h>

// Callbacks that run on a thread which is no longer waiting, after it has returned from a wait.
// The previous wait must not affect the result of whatever syscall ran the callback.
// Uses schedf() rather than checkpoint(): checkpoint() switches threads, which would hide
// state left behind by a wait that ended without one.

static SceUID cb;
static SceUID mainThread;
static SceUID flag;
static SceUID sema;
static SceUID freeSema;

static int cbFunc(int arg1, int arg2, void *arg) {
	schedf("  * cbFunc hit: %08x, %08x\n", arg1, arg2);
	return 0;
}

static int wakerFunc(SceSize argc, void *argv) {
	int action = *(int *)argv;
	sceKernelDelayThread(1000);
	switch (action) {
	case 0: sceKernelWakeupThread(mainThread); break;
	case 1: sceKernelSetEventFlag(flag, 1); break;
	case 2: sceKernelSignalSema(sema, 1); break;
	}
	return sceKernelExitDeleteThread(0);
}

static void startWaker(int action) {
	SceUID thread = sceKernelCreateThread("waker", &wakerFunc, 0x10, 0x1000, 0, NULL);
	sceKernelStartThread(thread, sizeof(action), &action);
}

static int exiterFunc(SceSize argc, void *argv) {
	sceKernelDelayThread(1000);
	return 0x1234;
}

static int wakeupCount() {
	SceKernelThreadInfo info;
	info.size = sizeof(info);
	sceKernelReferThreadStatus(0, &info);
	return info.wakeupCount;
}

// Get woken while running (not waiting), so the wakeup is queued.
static void queueWakeup() {
	startWaker(0);
	u32 start = sceKernelGetSystemTimeLow();
	while (sceKernelGetSystemTimeLow() - start < 3000) {
		continue;
	}
}

static void probeCallbacks(int which) {
	int result;
	sceKernelNotifyCallback(cb, 1);
	switch (which) {
	case 0:
		result = sceKernelCheckCallback();
		schedf("  sceKernelCheckCallback: %08x\n", result);
		break;
	case 1:
		result = sceKernelWaitSemaCB(freeSema, 1, NULL);
		schedf("  sceKernelWaitSemaCB (available): %08x\n", result);
		sceKernelSignalSema(freeSema, 1);
		break;
	case 2:
		result = sceKernelWaitEventFlagCB(flag, 0x100, PSP_EVENT_WAITOR, NULL, NULL);
		schedf("  sceKernelWaitEventFlagCB (set): %08x\n", result);
		break;
	case 3:
		result = sceUmdWaitDriveStatCB(PSP_UMD_NOT_PRESENT | PSP_UMD_PRESENT, 0);
		schedf("  sceUmdWaitDriveStatCB (matching): %08x\n", result);
		break;
	}
	result = sceKernelCheckCallback();
	schedf("  sceKernelCheckCallback (none): %08x\n", result);
}

typedef void (*PriorWait)();

static void testAfter(const char *title, PriorWait prior) {
	for (int which = 0; which < 4; ++which) {
		checkpointNext(which == 0 ? title : NULL);
		prior();
		probeCallbacks(which);
		flushschedf();
	}
}

static void noWait() {
}

static void eventFlagPending() {
	sceKernelSetEventFlag(flag, 3);
	sceKernelNotifyCallback(cb, 0x10);
	int result = sceKernelWaitEventFlagCB(flag, 3, PSP_EVENT_WAITAND | PSP_EVENT_WAITCLEAR, NULL, NULL);
	schedf("  sceKernelWaitEventFlagCB: %08x\n", result);
}

static void eventFlagWoken() {
	startWaker(1);
	int result = sceKernelWaitEventFlagCB(flag, 1, PSP_EVENT_WAITOR | PSP_EVENT_WAITCLEAR, NULL, NULL);
	schedf("  sceKernelWaitEventFlagCB: %08x\n", result);
}

static void eventFlagTimeout() {
	SceUInt timeout = 500;
	int result = sceKernelWaitEventFlag(flag, 1, PSP_EVENT_WAITOR, NULL, &timeout);
	schedf("  sceKernelWaitEventFlag: %08x\n", result);
}

static void semaPending() {
	sceKernelSignalSema(sema, 1);
	sceKernelNotifyCallback(cb, 0x11);
	int result = sceKernelWaitSemaCB(sema, 1, NULL);
	schedf("  sceKernelWaitSemaCB: %08x\n", result);
}

static void semaWoken() {
	startWaker(2);
	int result = sceKernelWaitSemaCB(sema, 1, NULL);
	schedf("  sceKernelWaitSemaCB: %08x\n", result);
}

static void semaTimeout() {
	SceUInt timeout = 500;
	int result = sceKernelWaitSemaCB(sema, 1, &timeout);
	schedf("  sceKernelWaitSemaCB: %08x\n", result);
}

static void delay() {
	int result = sceKernelDelayThreadCB(500);
	schedf("  sceKernelDelayThreadCB: %08x\n", result);
}

static void delayPending() {
	sceKernelNotifyCallback(cb, 0x12);
	int result = sceKernelDelayThreadCB(500);
	schedf("  sceKernelDelayThreadCB: %08x\n", result);
}

static void sleepWoken() {
	startWaker(0);
	int result = sceKernelSleepThreadCB();
	schedf("  sceKernelSleepThreadCB: %08x\n", result);
}

static void sleepPending() {
	queueWakeup();
	sceKernelNotifyCallback(cb, 0x13);
	int result = sceKernelSleepThreadCB();
	schedf("  sceKernelSleepThreadCB: %08x\n", result);
}

static void threadEnd() {
	SceUID exiter = sceKernelCreateThread("exiter", &exiterFunc, 0x10, 0x1000, 0, NULL);
	sceKernelStartThread(exiter, 0, NULL);
	int result = sceKernelWaitThreadEndCB(exiter, NULL);
	schedf("  sceKernelWaitThreadEndCB: %08x\n", result);
	sceKernelDeleteThread(exiter);
}

static void umdTimeout() {
	int result = sceUmdWaitDriveStatCB(0x08, 500);
	schedf("  sceUmdWaitDriveStatCB: %08x\n", result);
}

static void umdPending() {
	sceKernelNotifyCallback(cb, 0x14);
	int result = sceUmdWaitDriveStatCB(PSP_UMD_NOT_PRESENT | PSP_UMD_PRESENT, 0);
	schedf("  sceUmdWaitDriveStatCB: %08x\n", result);
}

extern "C" int main(int argc, char *argv[]) {
	mainThread = sceKernelGetThreadId();
	cb = sceKernelCreateCallback("afterwait", &cbFunc, NULL);
	flag = sceKernelCreateEventFlag("flag", 0, 0x100, NULL);
	sema = sceKernelCreateSema("sema", 0, 0, 1, NULL);
	freeSema = sceKernelCreateSema("free", 0, 1, 1, NULL);

	testAfter("No previous wait:", &noWait);
	testAfter("After sceKernelWaitEventFlagCB with a pending callback:", &eventFlagPending);
	testAfter("After sceKernelWaitEventFlagCB, woken:", &eventFlagWoken);
	testAfter("After sceKernelWaitEventFlag, timed out:", &eventFlagTimeout);
	testAfter("After sceKernelWaitSemaCB with a pending callback:", &semaPending);
	testAfter("After sceKernelWaitSemaCB, woken:", &semaWoken);
	testAfter("After sceKernelWaitSemaCB, timed out:", &semaTimeout);
	testAfter("After sceKernelDelayThreadCB:", &delay);
	testAfter("After sceKernelDelayThreadCB with a pending callback:", &delayPending);
	testAfter("After sceKernelSleepThreadCB, woken:", &sleepWoken);
	testAfter("After sceKernelSleepThreadCB with a pending callback:", &sleepPending);
	testAfter("After sceKernelWaitThreadEndCB:", &threadEnd);
	testAfter("After sceUmdWaitDriveStatCB, timed out:", &umdTimeout);
	testAfter("After sceUmdWaitDriveStatCB with a pending callback:", &umdPending);

	checkpointNext("Queued wakeup across a callback, after sceKernelSleepThreadCB:");
	sleepPending();
	queueWakeup();
	schedf("  wakeupCount before callback: %d\n", wakeupCount());
	sceKernelNotifyCallback(cb, 5);
	int result = sceKernelCheckCallback();
	schedf("  sceKernelCheckCallback: %08x\n", result);
	schedf("  wakeupCount after callback: %d\n", wakeupCount());
	schedf("  sceKernelCancelWakeupThread: %08x\n", sceKernelCancelWakeupThread(0));
	flushschedf();

	sceKernelDeleteSema(freeSema);
	sceKernelDeleteSema(sema);
	sceKernelDeleteEventFlag(flag);
	sceKernelDeleteCallback(cb);
	return 0;
}
