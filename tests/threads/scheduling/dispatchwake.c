#include <common.h>
#include <pspthreadman.h>

// Better threads made ready while dispatch is suspended: when do they run? Each thread appends its
// letter to a log when it gets the CPU; main appends M while suspended and R just after resuming.

static char order[64];
static int orderLen = 0;
static SceUID sema;
static volatile u32 ranAt;

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

static int delayer(SceSize argc, void *argp) {
	sceKernelDelayThread(1000);
	ranAt = sceKernelGetSystemTimeLow();
	mark('T');
	return 0;
}

static int semaWaiter(SceSize argc, void *argp) {
	sceKernelWaitSema(sema, 1, NULL);
	mark('T');
	return 0;
}

static int sleeper(SceSize argc, void *argp) {
	sceKernelSleepThread();
	mark('T');
	return 0;
}

static int runner(SceSize argc, void *argp) {
	mark('T');
	return 0;
}

static SceUID start(SceKernelThreadEntry entry) {
	SceUID t = sceKernelCreateThread("better", entry, 0x10, 0x1000, 0, NULL);
	sceKernelStartThread(t, 0, NULL);
	return t;
}

static void finish(SceUID t) {
	sceKernelWaitThreadEnd(t, NULL);
	sceKernelDeleteThread(t);
}

int main(int argc, char *argv[]) {
	sema = sceKernelCreateSema("sema", 0, 0, 1, NULL);

	checkpointNext("A better thread's 1000us delay ends while main has dispatch suspended for 3000us:");
	{
		SceUID t = start(&delayer);
		int state = sceKernelSuspendDispatchThread();
		u32 start = sceKernelGetSystemTimeLow();
		while (sceKernelGetSystemTimeLow() - start < 3000) {
			continue;
		}
		mark('M');
		u32 resumed = sceKernelGetSystemTimeLow();
		sceKernelResumeDispatchThread(state);
		mark('R');
		finish(t);
		report("Order");
		schedf("    it ran %s the resume\n", ranAt - resumed < 50 ? "just after" : (ranAt < resumed ? "before" : "well after"));
	}

	checkpointNext("Made ready while dispatch is suspended:");
	{
		SceUID t = start(&semaWaiter);
		int state = sceKernelSuspendDispatchThread();
		sceKernelSignalSema(sema, 1);
		mark('M');
		sceKernelResumeDispatchThread(state);
		mark('R');
		finish(t);
		report("Semaphore signalled");

		t = start(&sleeper);
		state = sceKernelSuspendDispatchThread();
		sceKernelWakeupThread(t);
		mark('M');
		sceKernelResumeDispatchThread(state);
		mark('R');
		finish(t);
		report("Woken up");

		state = sceKernelSuspendDispatchThread();
		t = start(&runner);
		mark('M');
		sceKernelResumeDispatchThread(state);
		mark('R');
		finish(t);
		report("Started");
	}

	checkpointNext("Waiting while dispatch is suspended:");
	{
		SceUID t = start(&semaWaiter);
		int state = sceKernelSuspendDispatchThread();
		sceKernelSignalSema(sema, 1);
		int result = sceKernelDelayThread(100);
		mark('M');
		sceKernelResumeDispatchThread(state);
		mark('R');
		finish(t);
		report("sceKernelDelayThread after waking a better thread");
		schedf("    it returned %08x\n", result);
	}

	flushschedf();
	return 0;
}
