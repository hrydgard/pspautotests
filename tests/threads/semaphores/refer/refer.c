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
	SceKernelSemaInfo semainfo;

	sema = sceKernelCreateSema("refer1", 0, 0, 1, NULL);

	// Crashes.
	//result = sceKernelReferSemaStatus(sema, NULL);
	//printf("%08X\n", result);

	result = sceKernelReferSemaStatus(sema, &semainfo);
	printf("%08X\n", result);

	sceKernelDeleteSema(sema);

	// Invalid.
	result = sceKernelReferSemaStatus(0, &semainfo);
	printf("%08X\n", result);

	// Deleted.
	result = sceKernelReferSemaStatus(sema, &semainfo);
	printf("%08X\n", result);

	// Verify scheduling order.
	SceUID sema1 = sceKernelCreateSema("refer1", 0, 0, 1, NULL);
	SceUID sema2 = sceKernelCreateSema("refer2", 0, 0, 1, NULL);
	printf("A");
	SceUID thread = sceKernelCreateThread("referTest", (void *)&threadFunc, 0x12, 0x10000, 0, NULL);
	sceKernelStartThread(thread, sizeof(int), &sema1);
	sceKernelDeleteSema(sema2);
	printf("C");
	sceKernelDeleteSema(sema1);
	printf("E\n");
}