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

int main(int argc, char **argv) {
	int result;
	SceUID sema;

	// Basic signal.
	sema = sceKernelCreateSema("signal", 0, 0, 1, NULL);
	PRINT_SEMAPHORE(sema);
	result = sceKernelSignalSema(sema, 2);
	printf("%08X\n", result);
	PRINT_SEMAPHORE(sema);

	// Signal a second time.
	result = sceKernelSignalSema(sema, 1);
	printf("%08X\n", result);
	PRINT_SEMAPHORE(sema);

	// Negative signal.
	result = sceKernelSignalSema(sema, -1);
	printf("%08X\n", result);
	PRINT_SEMAPHORE(sema);

	// Negative signal back to 0.
	result = sceKernelSignalSema(sema, -2);
	printf("%08X\n", result);
	PRINT_SEMAPHORE(sema);

	// Zero signal.
	result = sceKernelSignalSema(sema, 0);
	printf("%08X\n", result);
	PRINT_SEMAPHORE(sema);

	sceKernelDeleteSema(sema);

	// Signal a semaphore started negative.
	sema = sceKernelCreateSema("signal", 0, -3, 3, NULL);
	PRINT_SEMAPHORE(sema);
	result = sceKernelSignalSema(sema, 1);
	printf("%08X\n", result);
	PRINT_SEMAPHORE(sema);

	sceKernelDeleteSema(sema);

	// Verify scheduling order.
	SceUID sema1 = sceKernelCreateSema("signal1", 0, 0, 1, NULL);
	SceUID sema2 = sceKernelCreateSema("signal2", 0, 0, 1, NULL);
	printf("A");
	SceUID thread = sceKernelCreateThread("signalTest", (void *)&threadFunc, 0x12, 0x10000, 0, NULL);
	sceKernelStartThread(thread, sizeof(int), &sema1);
	sceKernelSignalSema(sema2, 1);
	printf("C");
	sceKernelSignalSema(sema1, 1);
	printf("E\n");
	sceKernelDeleteSema(sema1);
	sceKernelDeleteSema(sema2);

	// Signal invalid sema.
	result = sceKernelSignalSema(0, 1);
	printf("%08X\n", result);

	// Signal deleted sema.
	result = sceKernelSignalSema(sema, 1);
	printf("%08X\n", result);
}