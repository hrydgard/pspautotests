#include <common.h>
#include <pspthreadman.h>
#include <pspintrman.h>

static SceUID timers[8];
static volatile int ran[16];
static volatile int ranCount = 0;
// The handler deletes this timer, if it's the one running.
static volatile int deleteIndex = -1;
static volatile int deleteResult = 0;

static SceUInt vtimerHandler(SceUID uid, SceKernelSysClock *schedule, SceKernelSysClock *current, void *common) {
	int index = (int)common;
	if (ranCount < 16) {
		// Negative if the handler got another timer's id.
		ran[ranCount++] = uid == timers[index] ? index : -index;
	}
	if (index == deleteIndex) {
		deleteResult = sceKernelDeleteVTimer(uid);
		timers[index] = -1;
	}
	return 0;
}

// With interrupts suspended, a timer that comes due stays pending.
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

	flag = sceKernelCpuSuspendIntr();
	startTimer(1, 100);
	startTimer(2, 200);
	startTimer(3, 300);
	spin(2000);
	result = sceKernelDeleteVTimer(timers[2]);
	timers[2] = -1;
	sceKernelCpuResumeIntr(flag);
	// Nothing reaches the cable with interrupts suspended.
	printf("Delete the second while pending: %08x\n", result);
	sceKernelDelayThread(5000);
	printRan("Second of three deleted");
	deleteTimers();

	flag = sceKernelCpuSuspendIntr();
	startTimer(1, 100);
	startTimer(2, 200);
	spin(2000);
	result = sceKernelDeleteVTimer(timers[1]);
	timers[1] = -1;
	sceKernelCpuResumeIntr(flag);
	printf("Delete the first while pending: %08x\n", result);
	sceKernelDelayThread(5000);
	printRan("First of two deleted");
	deleteTimers();

	deleteIndex = 1;
	flag = sceKernelCpuSuspendIntr();
	startTimer(1, 100);
	startTimer(2, 200);
	spin(2000);
	sceKernelCpuResumeIntr(flag);
	sceKernelDelayThread(5000);
	printRan("Handler deletes its own timer");
	printf("Delete in handler: %08x\n", deleteResult);
	deleteIndex = -1;
	deleteTimers();

	startTimer(4, 100);
	sceKernelDelayThread(5000);
	printRan("Timer after");
	deleteTimers();

	return 0;
}
