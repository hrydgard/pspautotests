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
	int waitThreads;

	sema = sceKernelCreateSema("cancel", 0, 0, 1, NULL);

	// Cancel with null waitThreads.
	result = sceKernelCancelSema(sema, 1, NULL);
	printf("%08X\n", result);

	// Cancel with greater than max init, no waitThreads.
	result = sceKernelCancelSema(sema, 3, NULL);
	printf("%08X\n", result);

	// Cancel with greater than max init, with waitThreads.
	waitThreads = 99;
	result = sceKernelCancelSema(sema, 3, &waitThreads);
	printf("%08X - %d\n", result, waitThreads);

	// Cancel with negative init, no waitThreads.
	result = sceKernelCancelSema(sema, -3, NULL);
	printf("%08X\n", result);

	// Cancel with -1 init, no waitThreads.
	result = sceKernelCancelSema(sema, -1, NULL);
	printf("%08X\n", result);
	PRINT_SEMAPHORE(sema);

	sceKernelDeleteSema(sema);

	// Verify scheduling order.
	printf("A");
	sema = sceKernelCreateSema("cancel", 0, 0, 1, NULL);
	SceUID thread = sceKernelCreateThread("cancelTest", (void *)&threadFunc, 0x12, 0x10000, 0, NULL);
	sceKernelStartThread(thread, sizeof(int), &sema);
	result = sceKernelCancelSema(sema, 0, &waitThreads);
	printf("%08X - %d\n", result, waitThreads);
	printf("C");
	result = sceKernelCancelSema(sema, 1, &waitThreads);
	printf("%08X - %d\n", result, waitThreads);
	printf("E\n");
	PRINT_SEMAPHORE(sema);
	sceKernelDeleteSema(sema);

	// Cancel NULL?
	result = sceKernelCancelSema(0, 0, NULL);
	printf("%08X\n", result);

	// Cancel deleted?
	result = sceKernelCancelSema(sema, 0, NULL);
	printf("%08X\n", result);
}