#include "shared.h"

int cbFunc(int arg1, int arg2, void *arg) {
	checkpoint(" * cbFunc hit: %08x, %08x, %08x", arg1, arg2, arg);
	return 0;
}

inline void testDelete(const char *title, SceUID cb) {
	int result = sceKernelDeleteCallback(cb);
	if (result == 0) {
		checkpoint("%s: OK", title);
	} else {
		checkpoint("%s: Failed (%08x)", title, result);
	}
}

struct CallbackDeleter : public BasicThread {
	CallbackDeleter(const char *name, SceUID cb, int prio = 0x60)
		: BasicThread(name, prio), cb_(cb) {
		start();
	}

	virtual int execute() {
		testDelete("  Delete on different thread", cb_);
		return 0;
	}

	SceUID cb_;
};

struct CallbackWaiter : public BasicThread {
	CallbackWaiter(const char *name, int prio = 0x60)
		: BasicThread(name, prio) {
		start();
	}

	static int callback(int arg1, int arg2, void *arg) {
		CallbackWaiter *me = (CallbackWaiter *)arg;
		return me->hit(arg1, arg2);
	}

	int hit(int arg1, int arg2) {
		checkpoint("  Callback hit %08x, %08x", arg1, arg2);
		return 0;
	}

	virtual int execute() {
		cb_ = sceKernelCreateCallback(name_, &CallbackWaiter::callback, (void *)this);
		checkpoint("  Beginning sleep on %s", name_);
		checkpoint("  Woke from sleep: %08x", sceKernelSleepThreadCB());
		return 0;
	}

	SceUID callbackID() {
		return cb_;
	}

	SceUID cb_;
};

extern "C" int main(int argc, char *argv[]) {
	SceUID cb = sceKernelCreateCallback("delete", &cbFunc, NULL);

	checkpointNext("Objects:");
	testDelete("  Normal", cb);
	testDelete("  NULL", 0);
	testDelete("  Invalid", 0xDEADBEEF);
	testDelete("  Deleted", cb);

	checkpointNext("Different thread:");
	{
		cb = sceKernelCreateCallback("delete", &cbFunc, NULL);
		CallbackDeleter deleter("deleting thread", cb);
		sceKernelDelayThread(4000);
	}

	checkpointNext("Deleting during wait:");
	{
		CallbackWaiter waiter1("better priority sleeping thread", 0x10);
		CallbackWaiter waiter2("worse priority sleeping thread", 0x30);
		sceKernelDelayThread(4000);
		testDelete("  Delete better priority cb", waiter1.callbackID());
		testDelete("  Delete worse priority cb", waiter2.callbackID());
		sceKernelDelayThread(4000);
	}

	cb = sceKernelCreateCallback("delete", &cbFunc, NULL);
	checkpointNext("Delete while notified:");
	checkpoint("  Notify: %08x", sceKernelNotifyCallback(cb, 0x1337));
	testDelete("  Delete", cb);

	// And now just to double check - we don't get called, right?
	sceKernelDelayThreadCB(10000);

	return 0;
}
