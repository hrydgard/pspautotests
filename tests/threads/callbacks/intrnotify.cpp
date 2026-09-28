#include "shared.h"
#include <pspintrman.h>

// sceKernelNotifyCallback from an interrupt handler (vblank), and when the callback then runs.

static volatile SceUID notifyTarget = 0;
static volatile int notifyResult = 0x1337;
static volatile int notified = 0;
static volatile int callbackRan = 0;
static volatile int callbackRanDuringLoop = 0;

static void vblankHandler(int no, void *arg) {
	if (notifyTarget != 0) {
		notifyResult = sceKernelNotifyCallback(notifyTarget, 0x42);
		notifyTarget = 0;
		notified = 1;
	}
}

static void busyWait(u32 us) {
	u32 start = sceKernelGetSystemTimeLow();
	while (sceKernelGetSystemTimeLow() - start < us) {
		if (callbackRan && !callbackRanDuringLoop) {
			callbackRanDuringLoop = 1;
		}
	}
}

static int cbFunc(int arg1, int arg2, void *arg) {
	callbackRan = 1;
	schedf("  * callback on %s: %08x, %08x\n", (const char *)arg, arg1, arg2);
	return 0;
}

static void arm(SceUID cb) {
	notified = 0;
	callbackRan = 0;
	callbackRanDuringLoop = 0;
	notifyResult = 0x1337;
	notifyTarget = cb;
}

static void report(SceUID cb) {
	SceKernelCallbackInfo info;
	info.size = sizeof(info);
	sceKernelReferCallbackStatus(cb, &info);
	schedf("  notified=%d, result=%08x, ran=%d, ran during busy wait=%d, count=%d\n", notified, notifyResult, callbackRan, callbackRanDuringLoop, info.notifyCount);
}

struct Waiter : public BasicThread {
	Waiter(const char *name, int prio) : BasicThread(name, prio), cb_(0) {
		start();
	}

	virtual int execute() {
		cb_ = sceKernelCreateCallback(name_, &cbFunc, (void *)name_);
		int result = sceKernelSleepThreadCB();
		schedf("  %s woke: %08x\n", name_, result);
		sceKernelDeleteCallback(cb_);
		return 0;
	}

	SceUID cb_;
};

static int wakerFunc(SceSize argc, void *argv) {
	SceUID thread = *(SceUID *)argv;
	sceKernelDelayThread(50000);
	sceKernelWakeupThread(thread);
	return sceKernelExitDeleteThread(0);
}

extern "C" int main(int argc, char *argv[]) {
	SceUID mainThread = sceKernelGetThreadId();
	SceUID cb = sceKernelCreateCallback("main", &cbFunc, (void *)"main");
	sceKernelRegisterSubIntrHandler(PSP_VBLANK_INT, 0, (void *)&vblankHandler, NULL);
	sceKernelEnableSubIntr(PSP_VBLANK_INT, 0);

	checkpointNext("Own callback, sleeping with sceKernelSleepThreadCB:");
	{
		SceUID waker = sceKernelCreateThread("waker", &wakerFunc, 0x10, 0x1000, 0, NULL);
		sceKernelStartThread(waker, sizeof(mainThread), &mainThread);
		arm(cb);
		int result = sceKernelSleepThreadCB();
		schedf("  sceKernelSleepThreadCB: %08x\n", result);
		report(cb);
		flushschedf();
	}

	checkpointNext("Own callback, sceKernelDelayThread:");
	arm(cb);
	sceKernelDelayThread(50000);
	report(cb);
	schedf("  sceKernelCheckCallback: %08x\n", sceKernelCheckCallback());
	flushschedf();

	checkpointNext("Own callback, busy waiting:");
	arm(cb);
	busyWait(50000);
	report(cb);
	schedf("  sceKernelCheckCallback: %08x\n", sceKernelCheckCallback());
	flushschedf();

	checkpointNext("Better priority waiter, main busy waiting:");
	{
		Waiter w("waiter", 0x10);
		sceKernelDelayThread(1000);
		arm(w.cb_);
		busyWait(50000);
		report(w.cb_);
		sceKernelWakeupThread(w.thread_);
		sceKernelDelayThread(1000);
		flushschedf();
	}

	checkpointNext("Worse priority waiter, main busy waiting:");
	{
		Waiter w("waiter", 0x30);
		sceKernelDelayThread(1000);
		arm(w.cb_);
		busyWait(50000);
		report(w.cb_);
		schedf("  main yielding\n");
		sceKernelDelayThread(1000);
		report(w.cb_);
		sceKernelWakeupThread(w.thread_);
		sceKernelDelayThread(1000);
		flushschedf();
	}

	sceKernelDisableSubIntr(PSP_VBLANK_INT, 0);
	sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, 0);
	sceKernelDeleteCallback(cb);
	return 0;
}
