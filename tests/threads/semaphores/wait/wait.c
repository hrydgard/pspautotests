#include <common.h>

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <psploadexec.h>

#define PRINT_SEMAPHORE(sema) { \
	if (sema > 0) { \
		SceKernelSemaInfo semainfo; \
		sceKernelReferSemaStatus(sema, &semainfo); \
		printf("Sema(Id=%d,Size=%d,Name='%s',Attr=%d,init=%d,cur=%d,max=%d,wait=%d)\n", (sema > 0) ? 1 : 0, semainfo.size, semainfo.name, semainfo.attr, semainfo.initCount, semainfo.currentCount, semainfo.maxCount, semainfo.numWaitThreads); \
	} else { \
		printf("Sema(Id=0,result=%x)\n", sema); \
	} \
}

static int threadFunc(int argSize, void* argPointer) {
	printf("B");
	sceKernelWaitSemaCB(*(int*) argPointer, 1, NULL);
	printf("D");
	return 0;
}

static int waitTestFunc(int argSize, void* argPointer) {
	sceKernelDelayThread(1000);
	printf("C");
	sceKernelSignalSema(*(int*) argPointer, 1);
	printf("D");
	return 0;
}

int main(int argc, char **argv) {
	int result;
	SceUID sema;
	SceUInt timeout;

	sema = sceKernelCreateSema("wait1", 0, 1, 1, NULL);

	// Wait with timeout, signaled.
	timeout = 500;
	result = sceKernelWaitSema(sema, 1, &timeout);
	printf("%08X - %d\n", result, timeout);

	// Wait with timeout, greater than max.
	timeout = 500;
	result = sceKernelWaitSema(sema, 100, &timeout);
	printf("%08X - %d\n", result, timeout);

	// Wait with timeout, never signaled.
	timeout = 500;
	result = sceKernelWaitSema(sema, 1, &timeout);
	printf("%08X - %d\n", result, timeout);
	
	// Signaled off thread.
	printf("A");
	timeout = 5000;
	SceUID thread = sceKernelCreateThread("waitTest", (void *)&waitTestFunc, 0x12, 0x10000, 0, NULL);
	sceKernelStartThread(thread, sizeof(int), &sema);
	printf("B");
	result = sceKernelWaitSema(sema, 1, &timeout);
	printf("E\n");
	printf("%08X - %d\n", result, timeout / 1000);

	// Wait with timeout, negative desired.
	timeout = 500;
	result = sceKernelWaitSema(sema, -1, &timeout);
	printf("%08X - %d\n", result, timeout);
	PRINT_SEMAPHORE(sema);

	// Wait with timeout, zero desired.
	timeout = 500;
	result = sceKernelWaitSema(sema, 0, &timeout);
	printf("%08X - %d\n", result, timeout);

	// Wait without timeout, signaled.
	sceKernelSignalSema(sema, 1);
	result = sceKernelWaitSema(sema, 1, NULL);
	printf("%08X\n", result);

	// Wait without timeout, greater than max.
	result = sceKernelWaitSema(sema, 100, NULL);
	printf("%08X\n", result);

	sceKernelDeleteSema(sema);

	// Wait NULL?
	result = sceKernelWaitSema(0, 0, NULL);
	printf("%08X\n", result);

	// Wait deleted?
	result = sceKernelWaitSema(sema, 0, NULL);
	printf("%08X\n", result);

	SceUID sema1 = sceKernelCreateSema("wait1", 0, 0, 1, NULL);
	SceUID sema2 = sceKernelCreateSema("wait2", 0, 1, 1, NULL);

	// Verify scheduling order.
	printf("A");
	thread = sceKernelCreateThread("waitTest", (void *)&threadFunc, 0x12, 0x10000, 0, NULL);
	sceKernelStartThread(thread, sizeof(int), &sema1);
	sceKernelWaitSema(sema2, 1, NULL);
	printf("C");
	sceKernelDeleteSema(sema1);
	printf("E\n");
	sceKernelDeleteSema(sema2);
}