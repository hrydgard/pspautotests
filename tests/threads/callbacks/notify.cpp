#include "shared.h"

#include <psppower.h>

int cbFunc(int arg1, int arg2, void *arg) {
	checkpoint(" * cbFunc hit: %08x, %08x, %08x", arg1, arg2, arg);
	return 0;
}

inline void testNotify(const char *title, SceUID cb, int arg) {
	int result = sceKernelNotifyCallback(cb, arg);
	if (result == 0) {
		checkpoint("%s: OK", title);
	} else {
		checkpoint("%s: Failed (%08x)", title, result);
	}
}

struct CallbackSleeper : public BasicThread {
	CallbackSleeper(const char *name, int prio = 0x60, bool doStart = true)
		: BasicThread(name, prio), ret_(0) {
		if (doStart) {
			start();
		}
	}

	static int callback(int arg1, int arg2, void *arg) {
		CallbackSleeper *me = (CallbackSleeper *)arg;
		return me->hit(arg1, arg2);
	}

	virtual int hit(int arg1, int arg2) {
		checkpoint("  Callback hit %08x, %08x, returning %08x", arg1, arg2, ret_);
		return ret_;
	}

	virtual int execute() {
		cb_ = sceKernelCreateCallback(name_, &CallbackSleeper::callback, (void *)this);
		checkpoint("  Beginning sleep on %s", name_);
		checkpoint("  Woke from sleep: %08x", sceKernelSleepThreadCB());
		return 0;
	}

	SceUID callbackID() {
		return cb_;
	}

	void wakeup() {
		sceKernelWakeupThread(thread_);
	}

	void setReturn(int ret) {
		ret_ = ret;
	}

	int ret_;
	SceUID cb_;
};

struct SelfNotifier : public CallbackSleeper {
	SelfNotifier(const char *name, int prio = 0x60)
		: CallbackSleeper(name, prio, false), done_(false) {
		start();
	}

	virtual int hit(int arg1, int arg2) {
		checkpoint("  Callback hit %08x, %08x, returning %08x", arg1, arg2, ret_);
		if (!done_) {
			done_ = true;
			checkpoint("  Notifying self within callback: %08x", sceKernelNotifyCallback(cb_, 0xABCD));
			checkpoint("  sceKernelDelayThreadCB within callback: %08x", sceKernelDelayThreadCB(1000));
		}
		return ret_;
	}

	bool done_;
};

extern "C" int main(int argc, char *argv[]) {
	SceUID cb = sceKernelCreateCallback("notify", &cbFunc, NULL);

	checkpointNext("Objects:");
	testNotify("  Normal", cb, 0x1337);
	testNotify("  NULL", 0, 0x1337);
	testNotify("  Invalid", 0xDEADBEEF, 0x1337);
	sceKernelDeleteCallback(cb);
	testNotify("  Deleted", cb, 0x1337);

	cb = sceKernelCreateCallback("notify", &cbFunc, NULL);
	checkpointNext("Values:");
	testNotify("  Zero", cb, 0);
	testNotify("  DEADBEEF", cb, 0xDEADBEEF);

	checkpointNext("Notifies:");
	int result = 0;
	for (int i = 0; i < 10000; ++i) {
		result = sceKernelNotifyCallback(cb, 1);
		if (result != 0) {
			checkpoint("  Failed at %d: %08x", i, result);
			break;
		}
	}
	if (result == 0) {
		checkpoint("  10000 notifies: OK");
	}

	checkpoint("sceKernelDelayThreadCB: %08x", sceKernelDelayThreadCB(1000));

	checkpointNext("Different thread:");
	{
		CallbackSleeper waiter1("better priority sleeping thread", 0x10);
		CallbackSleeper waiter2("worse priority sleeping thread", 0x30);
		sceKernelDelayThread(1000);
		sceKernelNotifyCallback(waiter1.callbackID(), 0x1337);
		sceKernelNotifyCallback(waiter2.callbackID(), 0x1337);
		sceKernelDelayThread(1000);
	}

	checkpointNext("Return value:");
	{
		CallbackSleeper waiter("sleeping thread");
		waiter.setReturn(0x1337);
		sceKernelDelayThread(1000);
		testNotify("  Notify #1", waiter.callbackID(), 0x1337);
		sceKernelDelayThread(1000);
		testNotify("  Notify #2", waiter.callbackID(), 0x1337);
		sceKernelDelayThread(1000);
	}

	checkpointNext("Recursion:");
	{
		SelfNotifier waiter("sleeping thread");
		sceKernelDelayThread(1000);
		testNotify("  Notify #1", waiter.callbackID(), 0x1337);
		sceKernelDelayThread(1000);
	}

	checkpointNext("Mixing types:");
	checkpoint("  scePowerRegisterCallback (causes notify): %08x", scePowerRegisterCallback(0, cb));
	testNotify("  Manual notify", cb, 0x1337);
	checkpoint("  sceKernelDelayThreadCB: %08x", sceKernelDelayThreadCB(1000));

	return 0;
}
