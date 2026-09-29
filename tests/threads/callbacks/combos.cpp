#include "shared.h"

// Callbacks meeting other things: the order several run in when another thread notifies them, a
// wait that is released and has a callback notified before its thread gets to run, and what a
// callback costs.

static char order[64];
static int orderLen = 0;
static SceUID cbs[3];
static SceUID sema;
static volatile u32 callbackAt;
static volatile int waitResult;

static void mark(char c) {
	if (orderLen < (int)sizeof(order) - 1) {
		order[orderLen++] = c;
		order[orderLen] = 0;
	}
}

static void report(const char *title) {
	schedf("  %s: %s\n", title, order);
	orderLen = 0;
	order[0] = 0;
}

static const char *bucket(u32 us) {
	if (us < 10) {
		return "<10us";
	} else if (us < 25) {
		return "10-25us";
	} else if (us < 50) {
		return "25-50us";
	}
	return "over 50us";
}

static int orderCallback(int arg1, int arg2, void *arg) {
	mark((char)(int)arg);
	return 0;
}

static int timedCallback(int arg1, int arg2, void *arg) {
	callbackAt = sceKernelGetSystemTimeLow();
	return 0;
}

// Creates three callbacks (as its own, in the order A, B, C), then does the wait it's told to.
static int waiter(SceSize argc, void *argp) {
	int mode = *(int *)argp;
	for (int i = 0; i < 3; ++i) {
		cbs[i] = sceKernelCreateCallback("cb", mode == 2 ? &timedCallback : &orderCallback, (void *)(int)('A' + i));
	}
	if (mode == 0) {
		sceKernelSleepThreadCB();
	} else if (mode == 1) {
		waitResult = sceKernelWaitSemaCB(sema, 1, NULL);
		mark('w');
	} else {
		for (int i = 0; i < 1000; ++i) {
			sceKernelSleepThreadCB();
		}
	}
	return 0;
}

static SceUID startWaiter(int mode, int prio) {
	SceUID t = sceKernelCreateThread("waiter", &waiter, prio, 0x1000, 0, NULL);
	sceKernelStartThread(t, sizeof(mode), &mode);
	// Let it create its callbacks and start waiting, if it's worse than us.
	sceKernelDelayThread(1000);
	orderLen = 0;
	order[0] = 0;
	return t;
}

static void stopWaiter(SceUID t) {
	sceKernelTerminateDeleteThread(t);
	for (int i = 0; i < 3; ++i) {
		sceKernelDeleteCallback(cbs[i]);
	}
}

extern "C" int main(int argc, char *argv[]) {
	sema = sceKernelCreateSema("sema", 0, 0, 1, NULL);

	checkpointNext("Order, notified C, A, B by main while a worse thread sleeps:");
	{
		SceUID t = startWaiter(0, 0x30);
		sceKernelNotifyCallback(cbs[2], 1);
		sceKernelNotifyCallback(cbs[0], 1);
		sceKernelNotifyCallback(cbs[1], 1);
		mark('M');
		sceKernelDelayThread(1000);
		report("Ran");
		stopWaiter(t);
	}

	checkpointNext("Order, notified B, C, A twice each:");
	{
		SceUID t = startWaiter(0, 0x30);
		for (int k = 0; k < 2; ++k) {
			sceKernelNotifyCallback(cbs[1], 1);
			sceKernelNotifyCallback(cbs[2], 1);
			sceKernelNotifyCallback(cbs[0], 1);
		}
		mark('M');
		sceKernelDelayThread(1000);
		report("Ran");
		stopWaiter(t);
	}

	checkpointNext("Released and notified before a worse waiter runs (w is the wait returning):");
	{
		SceUID t = startWaiter(1, 0x30);
		sceKernelSignalSema(sema, 1);
		sceKernelNotifyCallback(cbs[0], 1);
		mark('M');
		sceKernelDelayThread(1000);
		report("Signal, then notify");
		schedf("    wait result %08x\n", waitResult);
		stopWaiter(t);

		t = startWaiter(1, 0x30);
		sceKernelNotifyCallback(cbs[0], 1);
		sceKernelSignalSema(sema, 1);
		mark('M');
		sceKernelDelayThread(1000);
		report("Notify, then signal");
		schedf("    wait result %08x\n", waitResult);
		stopWaiter(t);
	}

	checkpointNext("Cost, a better thread sleeping with sceKernelSleepThreadCB:");
	{
		SceUID t = startWaiter(2, 0x10);
		u32 toCallback = 0xFFFFFFFF, back = 0xFFFFFFFF;
		for (int i = 0; i < 8; ++i) {
			u32 start = sceKernelGetSystemTimeLow();
			sceKernelNotifyCallback(cbs[0], 1);
			u32 end = sceKernelGetSystemTimeLow();
			toCallback = callbackAt - start < toCallback ? callbackAt - start : toCallback;
			back = end - callbackAt < back ? end - callbackAt : back;
		}
		schedf("  Notify to callback %s, callback return to back in main %s\n", bucket(toCallback), bucket(back));
		stopWaiter(t);
	}

	flushschedf();
	return 0;
}
