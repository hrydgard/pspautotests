#include "shared.h"

// The same callback running inside itself, from CB waits within it.
// Only one level of nesting works: on hardware, a CB wait at depth 2 that finds a callback to run
// (the same one or not) never returns, even with a timeout. So nothing here goes deeper.

extern "C" int sceKernelCancelSema(SceUID semaid, int count, int *numWaitThreads);

static SceUID cb;
static SceUID sema;
static int mode;
static int depth;

static int helperFunc(SceSize argc, void *argv) {
	int at = *(int *)argv;
	sceKernelDelayThread(at);
	schedf("  helper signaling\n");
	sceKernelSignalSema(sema, 1);
	return sceKernelExitDeleteThread(0);
}

static void startHelper(int at) {
	SceUID thread = sceKernelCreateThread("helper", &helperFunc, 0x10, 0x1000, 0, NULL);
	sceKernelStartThread(thread, sizeof(at), &at);
}

static int cbFunc(int arg1, int arg2, void *arg) {
	int me = ++depth;
	schedf("  enter depth %d: %08x, %08x\n", me, arg1, arg2);
	int result;
	switch (mode) {
	case 0:
		if (me == 1) {
			sceKernelNotifyCallback(cb, me + 1);
		}
		result = sceKernelDelayThreadCB(1000);
		schedf("  depth %d sceKernelDelayThreadCB: %08x\n", me, result);
		break;
	case 1:
	case 2:
		if (me == 1) {
			sceKernelNotifyCallback(cb, me + 1);
		}
		{
			SceUInt timeout = me == 1 ? 6000 : 3000;
			result = sceKernelWaitSemaCB(sema, 1, &timeout);
			schedf("  depth %d sceKernelWaitSemaCB: %08x (timeout left %s)\n", me, result, timeout == 0 ? "0" : "some");
		}
		break;
	}
	schedf("  leave depth %d\n", me);
	--depth;
	return 0;
}

extern "C" int main(int argc, char *argv[]) {
	cb = sceKernelCreateCallback("recursion", &cbFunc, NULL);
	sema = sceKernelCreateSema("sema", 0, 0, 1, NULL);

	checkpointNext("sceKernelDelayThreadCB at both depths:");
	mode = 0;
	sceKernelNotifyCallback(cb, 1);
	schedf("  sceKernelCheckCallback: %08x\n", sceKernelCheckCallback());
	flushschedf();

	checkpointNext("Same semaphore at each depth, timing out:");
	mode = 1;
	sceKernelNotifyCallback(cb, 1);
	schedf("  sceKernelCheckCallback: %08x\n", sceKernelCheckCallback());
	flushschedf();

	checkpointNext("Same semaphore at each depth, signaled once:");
	mode = 2;
	sceKernelCancelSema(sema, 0, NULL);
	startHelper(1000);
	sceKernelNotifyCallback(cb, 1);
	schedf("  sceKernelCheckCallback: %08x\n", sceKernelCheckCallback());
	flushschedf();

	checkpointNext("Same semaphore at each depth, signaled twice:");
	sceKernelCancelSema(sema, 0, NULL);
	startHelper(1000);
	startHelper(2000);
	sceKernelNotifyCallback(cb, 1);
	schedf("  sceKernelCheckCallback: %08x\n", sceKernelCheckCallback());
	sceKernelDelayThread(10000);
	flushschedf();

	sceKernelDeleteSema(sema);
	sceKernelDeleteCallback(cb);
	return 0;
}
