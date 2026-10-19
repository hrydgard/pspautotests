#include <common.h>
#include <pspthreadman.h>
#include <pspintrman.h>

static volatile int ran[16];
static volatile int ranCount = 0;

static SceUInt alarmHandler(void *common) {
	if (ranCount < 16) {
		ran[ranCount++] = (int)common;
	}
	return 0;
}

// With interrupts suspended, an alarm that comes due stays pending.
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

int main(int argc, char **argv) {
	int flag, result;
	SceUID a, b;

	flag = sceKernelCpuSuspendIntr();
	a = sceKernelSetAlarm(100, alarmHandler, (void *)1);
	spin(2000);
	result = sceKernelCancelAlarm(a);
	sceKernelCpuResumeIntr(flag);
	// Nothing reaches the cable with interrupts suspended.
	printf("Cancel while pending: %08x\n", result);
	sceKernelDelayThread(5000);
	printRan("Cancelled while pending");

	sceKernelSetAlarm(100, alarmHandler, (void *)2);
	sceKernelDelayThread(5000);
	printRan("Next alarm");

	flag = sceKernelCpuSuspendIntr();
	a = sceKernelSetAlarm(100, alarmHandler, (void *)3);
	b = sceKernelSetAlarm(200, alarmHandler, (void *)4);
	spin(2000);
	result = sceKernelCancelAlarm(a);
	sceKernelCpuResumeIntr(flag);
	printf("Cancel first of two pending: %08x\n", result);
	sceKernelDelayThread(5000);
	printRan("First of two cancelled");
	sceKernelCancelAlarm(b);

	flag = sceKernelCpuSuspendIntr();
	a = sceKernelSetAlarm(100, alarmHandler, (void *)5);
	b = sceKernelSetAlarm(200, alarmHandler, (void *)6);
	spin(2000);
	result = sceKernelCancelAlarm(b);
	sceKernelCpuResumeIntr(flag);
	printf("Cancel second of two pending: %08x\n", result);
	sceKernelDelayThread(5000);
	printRan("Second of two cancelled");
	sceKernelCancelAlarm(a);

	sceKernelSetAlarm(100, alarmHandler, (void *)7);
	sceKernelDelayThread(5000);
	printRan("Alarm after");

	return 0;
}
