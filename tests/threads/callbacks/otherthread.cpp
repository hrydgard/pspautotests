#include "shared.h"

// Other threads' callbacks, while a thread is inside a callback.

static volatile int stopWaiters = 0;

struct LoopWaiter : public BasicThread {
	LoopWaiter(const char *name, int prio, int delayInCallback = 0)
		: BasicThread(name, prio), cb_(0), delayInCallback_(delayInCallback) {
		start();
	}

	static int callback(int arg1, int arg2, void *arg) {
		LoopWaiter *me = (LoopWaiter *)arg;
		schedf("  * callback on %s: %08x, %08x\n", me->name_, arg1, arg2);
		if (me->delayInCallback_ != 0) {
			sceKernelNotifyCallback(me->notifyInCallback_, 0x77);
			schedf("  %s callback delaying\n", me->name_);
			sceKernelDelayThread(me->delayInCallback_);
			schedf("  %s callback delayed\n", me->name_);
		}
		return 0;
	}

	virtual int execute() {
		cb_ = sceKernelCreateCallback(name_, &LoopWaiter::callback, (void *)this);
		while (!stopWaiters) {
			sceKernelSleepThreadCB();
		}
		sceKernelDeleteCallback(cb_);
		return 0;
	}

	void finish() {
		stopWaiters = 1;
		sceKernelWakeupThread(thread_);
		sceKernelDelayThread(1000);
		stopWaiters = 0;
	}

	SceUID cb_;
	int delayInCallback_;
	SceUID notifyInCallback_;
};

static LoopWaiter *target;
static int mode;

static void busyWait(u32 us) {
	u32 start = sceKernelGetSystemTimeLow();
	while (sceKernelGetSystemTimeLow() - start < us) {
		continue;
	}
}

static int mainCallback(int arg1, int arg2, void *arg) {
	schedf("  * main callback: %08x, %08x\n", arg1, arg2);
	if (target == NULL) {
		return 0;
	}
	schedf("  main callback notifying %s\n", target->name_);
	sceKernelNotifyCallback(target->cb_, 0x10 + mode);
	schedf("  main callback notified\n");
	switch (mode) {
	case 0:
		break;
	case 1:
		schedf("  sceKernelDelayThread in callback: %08x\n", sceKernelDelayThread(2000));
		break;
	case 2:
		schedf("  sceKernelDelayThreadCB in callback: %08x\n", sceKernelDelayThreadCB(2000));
		break;
	case 3:
		busyWait(2000);
		schedf("  busy wait in callback done\n");
		break;
	}
	schedf("  main callback returning\n");
	return 0;
}

static void testFromMainCallback(const char *title, SceUID mainCb, int prio) {
	static const char *modes[] = { "no wait", "sceKernelDelayThread", "sceKernelDelayThreadCB", "busy wait" };
	checkpointNext(title);
	for (mode = 0; mode < 4; ++mode) {
		LoopWaiter w("waiter", prio);
		target = &w;
		sceKernelDelayThread(1000);
		schedf(" %s:\n", modes[mode]);
		sceKernelNotifyCallback(mainCb, mode);
		int result = sceKernelCheckCallback();
		schedf("  sceKernelCheckCallback: %08x\n", result);
		sceKernelDelayThread(1000);
		schedf("  after yield\n");
		target = NULL;
		w.finish();
		flushschedf();
	}
}

extern "C" int main(int argc, char *argv[]) {
	// Main runs at 0x20.
	SceUID mainCb = sceKernelCreateCallback("main", &mainCallback, NULL);

	testFromMainCallback("Better priority waiter, notified from a callback:", mainCb, 0x10);
	testFromMainCallback("Worse priority waiter, notified from a callback:", mainCb, 0x30);

	checkpointNext("Main's callback, while a worse priority thread is in a callback:");
	{
		LoopWaiter w("waiter", 0x30, 2000);
		w.notifyInCallback_ = mainCb;
		sceKernelDelayThread(1000);
		sceKernelNotifyCallback(w.cb_, 1);
		schedf("  main sceKernelDelayThreadCB\n");
		int result = sceKernelDelayThreadCB(5000);
		schedf("  main sceKernelDelayThreadCB: %08x\n", result);
		w.finish();
		flushschedf();
	}

	checkpointNext("Main's callback, while a better priority thread is in a callback:");
	{
		LoopWaiter w("waiter", 0x10, 2000);
		w.notifyInCallback_ = mainCb;
		sceKernelDelayThread(1000);
		schedf("  main notifying waiter\n");
		sceKernelNotifyCallback(w.cb_, 1);
		schedf("  main sceKernelDelayThreadCB\n");
		int result = sceKernelDelayThreadCB(5000);
		schedf("  main sceKernelDelayThreadCB: %08x\n", result);
		w.finish();
		flushschedf();
	}

	checkpointNext("Two threads in callbacks at once:");
	{
		LoopWaiter w1("waiter 0x30", 0x30, 3000);
		LoopWaiter w2("waiter 0x28", 0x28, 3000);
		w1.notifyInCallback_ = mainCb;
		w2.notifyInCallback_ = mainCb;
		sceKernelDelayThread(1000);
		sceKernelNotifyCallback(w1.cb_, 1);
		sceKernelNotifyCallback(w2.cb_, 2);
		schedf("  main sceKernelDelayThread\n");
		sceKernelDelayThread(10000);
		schedf("  main sceKernelDelayThread done\n");
		sceKernelCheckCallback();
		w1.finish();
		w2.finish();
		flushschedf();
	}

	sceKernelDeleteCallback(mainCb);
	return 0;
}
