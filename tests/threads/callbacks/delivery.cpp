#include "shared.h"

// When does a notified callback of another thread run, relative to the notifying thread?

struct Waiter : public BasicThread {
	Waiter(const char *name, int prio, int mode)
		: BasicThread(name, prio), mode_(mode), cb_(0) {
		start();
	}

	static int callback(int arg1, int arg2, void *arg) {
		Waiter *me = (Waiter *)arg;
		schedf("  * callback on %s: %08x, %08x\n", me->name_, arg1, arg2);
		return 0;
	}

	virtual int execute() {
		// Callbacks belong to the thread that creates them.
		cb_ = sceKernelCreateCallback(name_, &Waiter::callback, (void *)this);
		int result;
		switch (mode_) {
		case 0: result = sceKernelSleepThreadCB(); break;
		case 1: result = sceKernelSleepThread(); break;
		default: result = sceKernelDelayThreadCB(10000); break;
		}
		schedf("  %s woke: %08x\n", name_, result);
		return 0;
	}

	~Waiter() {
		sceKernelDeleteCallback(cb_);
	}

	int mode_;
	SceUID cb_;
};

static void count(const char *title, SceUID cb) {
	SceKernelCallbackInfo info;
	info.size = sizeof(info);
	sceKernelReferCallbackStatus(cb, &info);
	schedf("  %s: count=%d, arg=%08x\n", title, info.notifyCount, info.notifyArg);
}

static void notify(const char *title, SceUID cb, int arg) {
	schedf("  notify %s\n", title);
	int result = sceKernelNotifyCallback(cb, arg);
	schedf("  notify %s returned %08x\n", title, result);
}

static void testPriority(const char *title, int prio, int mode) {
	checkpointNext(title);
	{
		Waiter w("waiter", prio, mode);
		sceKernelDelayThread(1000);
		notify("once", w.cb_, 0x10);
		count("after notify", w.cb_);
		notify("again", w.cb_, 0x11);
		count("after again", w.cb_);
		schedf("  main yielding\n");
		sceKernelDelayThread(1000);
		count("after yield", w.cb_);
		flushschedf();
		sceKernelWakeupThread(w.thread_);
		sceKernelDelayThread(1000);
		flushschedf();
	}
}

extern "C" int main(int argc, char *argv[]) {
	// Main runs at 0x20.
	testPriority("Better priority, SleepThreadCB:", 0x10, 0);
	testPriority("Same priority, SleepThreadCB:", 0x20, 0);
	testPriority("Worse priority, SleepThreadCB:", 0x30, 0);
	testPriority("Better priority, SleepThread:", 0x10, 1);
	testPriority("Worse priority, SleepThread:", 0x30, 1);
	testPriority("Better priority, DelayThreadCB:", 0x10, 2);

	checkpointNext("Worse priority, cancelled before yielding:");
	{
		Waiter w("waiter", 0x30, 0);
		sceKernelDelayThread(1000);
		notify("once", w.cb_, 0x20);
		schedf("  cancel: %08x\n", sceKernelCancelCallback(w.cb_));
		count("after cancel", w.cb_);
		sceKernelDelayThread(1000);
		count("after yield", w.cb_);
		flushschedf();
		sceKernelWakeupThread(w.thread_);
		sceKernelDelayThread(1000);
		flushschedf();
	}

	checkpointNext("Two better priority waiters:");
	{
		Waiter w1("waiter 0x18", 0x18, 0);
		Waiter w2("waiter 0x10", 0x10, 0);
		sceKernelDelayThread(1000);
		notify("0x18", w1.cb_, 0x30);
		notify("0x10", w2.cb_, 0x31);
		flushschedf();
		sceKernelWakeupThread(w1.thread_);
		sceKernelWakeupThread(w2.thread_);
		sceKernelDelayThread(1000);
		flushschedf();
	}

	checkpointNext("Two worse priority waiters:");
	{
		Waiter w1("waiter 0x30", 0x30, 0);
		Waiter w2("waiter 0x28", 0x28, 0);
		sceKernelDelayThread(1000);
		notify("0x30", w1.cb_, 0x40);
		notify("0x28", w2.cb_, 0x41);
		schedf("  main yielding\n");
		sceKernelDelayThread(1000);
		flushschedf();
		sceKernelWakeupThread(w1.thread_);
		sceKernelWakeupThread(w2.thread_);
		sceKernelDelayThread(1000);
		flushschedf();
	}

	checkpointNext("Better priority, dispatch suspended:");
	{
		Waiter w("waiter", 0x10, 0);
		sceKernelDelayThread(1000);
		int state = sceKernelSuspendDispatchThread();
		notify("once", w.cb_, 0x50);
		count("while suspended", w.cb_);
		schedf("  resume: %08x\n", sceKernelResumeDispatchThread(state));
		count("after resume", w.cb_);
		flushschedf();
		sceKernelWakeupThread(w.thread_);
		sceKernelDelayThread(1000);
		flushschedf();
	}

	return 0;
}
