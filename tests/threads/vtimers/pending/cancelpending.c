#include <common.h>
#include <pspthreadman.h>
#include <pspintrman.h>

// A vtimer that has come due (with interrupts suspended, so its interrupt is pending) and then has
// its handler cancelled, cleared or replaced, or the timer stopped, before interrupts are resumed.

static SceUID timers[8];
static volatile int ran[16];
static volatile int ranCount = 0;
// The handler for this index cancels the handler of cancelIndex.
static volatile int cancelFrom = -1;
static volatile int cancelIndex = -1;
static volatile int cancelResult = 0x12345678;

static SceUInt vtimerHandler(SceUID uid, SceKernelSysClock *schedule, SceKernelSysClock *current, void *common) {
	int index = (int)common;
	if (ranCount < 16) {
		ran[ranCount++] = uid == timers[index] ? index : -index;
	}
	if (index == cancelFrom) {
		cancelResult = sceKernelCancelVTimerHandler(timers[cancelIndex]);
	}
	return 0;
}

static SceUInt otherHandler(SceUID uid, SceKernelSysClock *schedule, SceKernelSysClock *current, void *common) {
	int index = (int)common;
	if (ranCount < 16) {
		ran[ranCount++] = 100 + index;
	}
	return 0;
}

static void spin(u32 us) {
	u32 start = sceKernelGetSystemTimeLow();
	while (sceKernelGetSystemTimeLow() - start < us) {
		continue;
	}
}

static void printRan(const char *title) {
	int i;
	printf("%s: %d ran", title, ranCount);
	for (i = 0; i < ranCount; ++i) {
		printf(" %d", ran[i]);
	}
	printf("\n");
	ranCount = 0;
}

static void startTimer(int index, u32 us) {
	SceKernelSysClock schedule;
	schedule.low = us;
	schedule.hi = 0;
	timers[index] = sceKernelCreateVTimer("pending", NULL);
	sceKernelSetVTimerHandler(timers[index], &schedule, vtimerHandler, (void *)index);
	sceKernelStartVTimer(timers[index]);
}

static void deleteTimers() {
	int i;
	for (i = 0; i < 8; ++i) {
		if (timers[i] > 0) {
			sceKernelDeleteVTimer(timers[i]);
		}
		timers[i] = -1;
	}
}

int main(int argc, char **argv) {
	int flag, result;
	SceKernelSysClock schedule;

	flag = sceKernelCpuSuspendIntr();
	startTimer(1, 100);
	startTimer(2, 200);
	startTimer(3, 300);
	spin(2000);
	result = sceKernelCancelVTimerHandler(timers[2]);
	sceKernelCpuResumeIntr(flag);
	printf("Cancel the second while pending: %08x\n", result);
	sceKernelDelayThread(5000);
	printRan("Second of three cancelled");
	deleteTimers();

	flag = sceKernelCpuSuspendIntr();
	startTimer(1, 100);
	startTimer(2, 200);
	spin(2000);
	schedule.low = 100000;
	schedule.hi = 0;
	result = sceKernelSetVTimerHandler(timers[1], &schedule, NULL, NULL);
	sceKernelCpuResumeIntr(flag);
	printf("Clear the first while pending: %08x\n", result);
	sceKernelDelayThread(5000);
	printRan("First of two cleared");
	deleteTimers();

	flag = sceKernelCpuSuspendIntr();
	startTimer(1, 100);
	startTimer(2, 200);
	spin(2000);
	schedule.low = 100000;
	schedule.hi = 0;
	result = sceKernelSetVTimerHandler(timers[1], &schedule, otherHandler, (void *)1);
	sceKernelCpuResumeIntr(flag);
	printf("Replace the first while pending: %08x\n", result);
	sceKernelDelayThread(5000);
	printRan("First of two replaced");
	deleteTimers();

	flag = sceKernelCpuSuspendIntr();
	startTimer(1, 100);
	startTimer(2, 200);
	spin(2000);
	result = sceKernelStopVTimer(timers[1]);
	sceKernelCpuResumeIntr(flag);
	printf("Stop the first while pending: %08x\n", result);
	sceKernelDelayThread(5000);
	printRan("First of two stopped");
	deleteTimers();

	cancelFrom = 1;
	cancelIndex = 2;
	flag = sceKernelCpuSuspendIntr();
	startTimer(1, 100);
	startTimer(2, 200);
	spin(2000);
	sceKernelCpuResumeIntr(flag);
	sceKernelDelayThread(5000);
	printRan("First cancels the second in its handler");
	printf("Cancel in handler: %08x\n", cancelResult);
	cancelFrom = -1;
	deleteTimers();

	startTimer(4, 100);
	sceKernelDelayThread(5000);
	printRan("Timer after");
	deleteTimers();

	return 0;
}
