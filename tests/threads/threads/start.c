#include "shared.h"

volatile int argLengthArgc = 0;
volatile void *argLengthArgv;
int argLengthFunc(SceSize argc, void *argv) {
	argLengthArgc = argc;
	argLengthArgv = argv;
	return 0;
}

int testFunc(SceSize argc, void *argv) {
	schedf("* delayFunc\n");

	sceKernelDelayThread(500);

	return 7;
}

int main(int argc, char *argv[]) {
	int i, result;
	SceUID testThread = sceKernelCreateThread("test", &testFunc, sceKernelGetThreadCurrentPriority(), 0x1000, 0, NULL);
	SceUID deletedThread = sceKernelCreateThread("deleted", &testFunc, sceKernelGetThreadCurrentPriority(), 0x1000, 0, NULL);
	sceKernelDeleteThread(deletedThread);

	checkpointNext("Thread IDs:");
	checkpoint("  Normal: %08x", sceKernelStartThread(testThread, 0, NULL));
	checkpoint("  Twice: %08x", sceKernelStartThread(testThread, 0, NULL));
	checkpoint("  NULL: %08x", sceKernelStartThread(0, 0, NULL));
	checkpoint("  Current: %08x", sceKernelStartThread(sceKernelGetThreadId(), 0, NULL));
	checkpoint("  Deleted: %08x", sceKernelStartThread(deletedThread, 0, NULL));
	checkpoint("  Invalid: %08x", sceKernelStartThread(0xDEADBEEF, 0, NULL));

	checkpointNext("Argument Length:");
	SceUID argLengthThread = sceKernelCreateThread("argLength", &argLengthFunc, sceKernelGetThreadCurrentPriority() - 1, 0x800, 0, NULL);
	char argLengthTemp[0x1000];

	result = sceKernelStartThread(argLengthThread, 8, NULL);
	sceKernelWaitThreadEnd(argLengthThread, NULL);
	checkpoint("  With NULL ptr: %08x (%d, %s)", result, argLengthArgc, argLengthArgv ==  0 ? "NULL" : (argLengthTemp == argLengthTemp ? "same ptr" : "unknown"));

	// Note: larger than stack crashes the PSP, and so does a bad arg ptr.
	static int lengths[] = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 80, 90, 0x600};
	for (i = 0; i < ARRAY_SIZE(lengths); ++i) {
		argLengthArgc = -1;
		result = sceKernelStartThread(argLengthThread, lengths[i], argLengthTemp);
		sceKernelWaitThreadEnd(argLengthThread, NULL);
		checkpoint("  %d arg length: %08x (%d, %s)", lengths[i], result, argLengthArgc, argLengthArgv ==  0 ? "NULL" : (argLengthTemp == argLengthTemp ? "same ptr" : "unknown"));
	}

	checkpointNext("Priorities:");
	static int priorities[] = {0x8, 0x1F, 0x20, 0x21, 0x40};
	for (i = 0; i < ARRAY_SIZE(priorities); ++i) {
		argLengthArgc = -1;
		sceKernelDeleteThread(argLengthThread);
		argLengthThread = sceKernelCreateThread("argLength", &argLengthFunc, priorities[i], 0x800, 0, NULL);
		result = sceKernelStartThread(argLengthThread, priorities[i], argLengthTemp);
		int afterStart = argLengthArgc;
		sceKernelWaitThreadEnd(argLengthThread, NULL);

		int priorityDiff = priorities[i] - sceKernelGetThreadCurrentPriority();
		checkpoint("  priority 0x%02x: %08x (current%s0x%02x, %s)", priorities[i], result, priorityDiff < 0 ? "-" : "+", priorityDiff < 0 ? -priorityDiff : priorityDiff, afterStart == priorities[i] ? "resched" : (argLengthArgc == priorities[i] ? "deferred" : "never"));
	}

	return 0;
}