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

	// Delete NULL?
	result = sceKernelDeleteSema(0);
	printf("%08X\n", result);

	// Delete invalid?
	result = sceKernelDeleteSema(0xDEADBEEF);
	printf("%08X\n", result);

	// Verify scheduling order.
	SceUID sema1 = sceKernelCreateSema("delete1", 0, 0, 1, NULL);
	SceUID sema2 = sceKernelCreateSema("delete2", 0, 0, 1, NULL);
	SceUID thread = sceKernelCreateThread("deleteTest", (void *)&threadFunc, 0x12, 0x10000, 0, NULL);

	printf("A");
	sceKernelStartThread(thread, sizeof(int), &sema1);
	sceKernelDeleteSema(sema2);
	printf("C");
	sceKernelDeleteSema(sema1);
	printf("E\n");

	// Delete twice?
	result = sceKernelDeleteSema(sema1);
	printf("%08X\n", result);
}