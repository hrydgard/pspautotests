#include "shared.h"

// Terminating a suspended thread: its status afterwards, and when threads waiting for it to end
// are woken.

static volatile int woke = 0;
static volatile int wokeResult = 0;

static int idleThread(SceSize argc, void *argp) {
	sceKernelDelayThread(100000);
	return 0x1234;
}

static int waitThread(SceSize argc, void *argp) {
	wokeResult = sceKernelWaitThreadEnd(*(SceUID *)argp, NULL);
	woke = 1;
	return 0;
}

static void printStatus(const char *title, SceUID thread) {
	SceKernelThreadInfo info;
	info.size = sizeof(info);
	int result = sceKernelReferThreadStatus(thread, &info);
	if (result < 0) {
		schedf("  %s: %08x\n", title, result);
	} else {
		schedf("  %s: status=%x, exitStatus=%08x\n", title, info.status, info.exitStatus);
	}
}

static void printWaiter() {
	schedf("  waiter %s", woke ? "woke" : "still waiting");
	if (woke) {
		schedf(" with %08x", wokeResult);
	}
	schedf("\n");
}

typedef int (*TermFunc)(SceUID);

static void test(const char *title, TermFunc term, int withWaiter) {
	checkpointNext(title);
	woke = 0;
	SceUID thread = sceKernelCreateThread("target", &idleThread, 0x30, 0x800, 0, NULL);
	sceKernelStartThread(thread, 0, NULL);
	sceKernelSuspendThread(thread);
	SceUID waiter = -1;
	if (withWaiter) {
		waiter = sceKernelCreateThread("waiter", &waitThread, 0x10, 0x800, 0, NULL);
		sceKernelStartThread(waiter, sizeof(thread), &thread);
	}
	printStatus("Before", thread);

	schedf("  Terminate: %08x\n", term(thread));
	printStatus("After terminate", thread);
	if (withWaiter) {
		sceKernelDelayThread(1000);
		printWaiter();
	}

	schedf("  Resume: %08x\n", sceKernelResumeThread(thread));
	printStatus("After resume", thread);
	if (withWaiter) {
		printWaiter();
		sceKernelDelayThread(1000);
		printWaiter();
	}

	schedf("  Start: %08x\n", sceKernelStartThread(thread, 0, NULL));
	sceKernelDelayThread(1000);
	printStatus("After start", thread);
	flushschedf();

	sceKernelTerminateDeleteThread(thread);
	if (waiter >= 0) {
		sceKernelTerminateDeleteThread(waiter);
	}
}

static void testStartSuspended() {
	checkpointNext("Starting a terminated thread that's still suspended:");
	SceUID thread = sceKernelCreateThread("target", &idleThread, 0x30, 0x800, 0, NULL);
	sceKernelStartThread(thread, 0, NULL);
	sceKernelSuspendThread(thread);
	sceKernelTerminateThread(thread);
	schedf("  Start: %08x\n", sceKernelStartThread(thread, 0, NULL));
	printStatus("After start", thread);
	schedf("  Resume: %08x\n", sceKernelResumeThread(thread));
	sceKernelDelayThread(1000);
	printStatus("After resume", thread);
	flushschedf();
	sceKernelTerminateDeleteThread(thread);
}

static int lowThread(SceSize argc, void *argp) {
	return 0;
}

// When does a higher priority thread that the terminate woke get to run?
static void testPreempt(const char *title, TermFunc term, int suspended) {
	checkpointNext(title);
	woke = 0;
	SceUID thread = sceKernelCreateThread("target", &idleThread, 0x30, 0x800, 0, NULL);
	sceKernelStartThread(thread, 0, NULL);
	if (suspended) {
		sceKernelSuspendThread(thread);
	}
	SceUID waiter = sceKernelCreateThread("waiter", &waitThread, 0x10, 0x800, 0, NULL);
	sceKernelStartThread(waiter, sizeof(thread), &thread);
	SceUID low = sceKernelCreateThread("low", &lowThread, 0x30, 0x800, 0, NULL);

	int result = term(thread);
	int afterTerm = woke;
	int result2 = sceKernelGetThreadId() > 0 ? 0 : -1;
	int afterSyscall = woke;
	int result3 = sceKernelStartThread(low, 0, NULL);
	int afterStart = woke;
	int result4 = sceKernelDeleteThread(thread);
	int afterDelete = woke;
	sceKernelDelayThread(1000);
	int afterDelay = woke;
	schedf("  Terminate: %08x, woke: right after %d, after a syscall %d, after starting a lower thread %d (%08x), after delete %d (%08x), after delay %d\n",
		result, afterTerm, afterSyscall, afterStart, result3, afterDelete, result4, afterDelay);
	(void)result2;
	flushschedf();

	sceKernelTerminateDeleteThread(thread);
	sceKernelTerminateDeleteThread(waiter);
	sceKernelTerminateDeleteThread(low);
}

static volatile int startedRan = 0;
static int startedThread(SceSize argc, void *argp) {
	startedRan = woke ? 2 : 1;
	return 0;
}

// Main runs at 0x20. Does starting another thread let the woken waiter (0x10) run?
static void testStartPreempt(const char *title, int prio) {
	checkpointNext(title);
	woke = 0;
	startedRan = 0;
	SceUID thread = sceKernelCreateThread("target", &idleThread, 0x30, 0x800, 0, NULL);
	sceKernelStartThread(thread, 0, NULL);
	SceUID waiter = sceKernelCreateThread("waiter", &waitThread, 0x10, 0x800, 0, NULL);
	sceKernelStartThread(waiter, sizeof(thread), &thread);
	SceUID other = sceKernelCreateThread("other", &startedThread, prio, 0x800, 0, NULL);

	sceKernelTerminateThread(thread);
	int result = sceKernelStartThread(other, 0, NULL);
	int afterStart = woke;
	int otherRan = startedRan;
	sceKernelDelayThread(1000);
	schedf("  Start: %08x, waiter woke after start %d, started thread ran %s\n", result, afterStart,
		otherRan == 0 ? "no" : (otherRan == 1 ? "before the waiter" : "after the waiter"));
	flushschedf();

	sceKernelTerminateDeleteThread(thread);
	sceKernelTerminateDeleteThread(waiter);
	sceKernelTerminateDeleteThread(other);
}

// Is the thread the terminate woke in the ready queue? Try calls that dispatch without starting
// anything.
static void testDispatchAfterTerm(const char *title, int mode) {
	checkpointNext(title);
	woke = 0;
	SceUID thread = sceKernelCreateThread("target", &idleThread, 0x30, 0x800, 0, NULL);
	sceKernelStartThread(thread, 0, NULL);
	SceUID waiter = sceKernelCreateThread("waiter", &waitThread, 0x10, 0x800, 0, NULL);
	sceKernelStartThread(waiter, sizeof(thread), &thread);
	SceUID sema = sceKernelCreateSema("sema", 0, 0, 1, NULL);

	sceKernelTerminateThread(thread);
	int result;
	switch (mode) {
	case 0: result = sceKernelRotateThreadReadyQueue(0); break;
	case 1: result = sceKernelChangeThreadPriority(0, 0x20); break;
	default: result = sceKernelSignalSema(sema, 1); break;
	}
	int after = woke;
	sceKernelDelayThread(1000);
	schedf("  Result: %08x, waiter woke after it %d\n", result, after);
	flushschedf();

	sceKernelDeleteSema(sema);
	sceKernelTerminateDeleteThread(thread);
	sceKernelTerminateDeleteThread(waiter);
}

int main(int argc, char *argv[]) {
	test("sceKernelTerminateThread, no waiter:", &sceKernelTerminateThread, 0);
	test("sceKernelTerminateThread, with a waiter:", &sceKernelTerminateThread, 1);
	testStartSuspended();
	testPreempt("Preemption, suspended target:", &sceKernelTerminateThread, 1);
	testPreempt("Preemption, running target:", &sceKernelTerminateThread, 0);
	testPreempt("Preemption, sceKernelTerminateDeleteThread:", &sceKernelTerminateDeleteThread, 0);
	testStartPreempt("Starting a thread of the same priority:", 0x20);
	testStartPreempt("Starting a thread of better priority:", 0x18);
	testStartPreempt("Starting a thread of better priority than the waiter:", 0x08);
	testDispatchAfterTerm("sceKernelRotateThreadReadyQueue(0):", 0);
	testDispatchAfterTerm("sceKernelChangeThreadPriority(0, 0x20):", 1);
	testDispatchAfterTerm("sceKernelSignalSema, no waiter:", 2);
	return 0;
}
