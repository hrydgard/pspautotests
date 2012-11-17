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

SceUID sema;

static int threadFunction(int argSize, void* argPointer) {
	int num = argPointer ? *((int*)argPointer) : 0;
	printf("A%d\n", num);
	sceKernelWaitSemaCB(sema, 1, NULL);
	printf("B%d\n", num);

	return 0;
}

void execPriorityTests(int attr) {
	int result;
	SceUID threads[5];
	int test[5] = {1, 2, 3, 4, 5};

	sema = sceKernelCreateSema("sema1", attr, 0, 2, NULL);
	PRINT_SEMAPHORE(sema);

	threads[0] = sceKernelCreateThread("Thread-0", (void *)&threadFunction, 0x16, 0x10000, 0, NULL);
	threads[1] = sceKernelCreateThread("Thread-1", (void *)&threadFunction, 0x15, 0x10000, 0, NULL);
	threads[2] = sceKernelCreateThread("Thread-2", (void *)&threadFunction, 0x14, 0x10000, 0, NULL);
	threads[3] = sceKernelCreateThread("Thread-3", (void *)&threadFunction, 0x13, 0x10000, 0, NULL);
	threads[4] = sceKernelCreateThread("Thread-4", (void *)&threadFunction, 0x12, 0x10000, 0, NULL);

	sceKernelStartThread(threads[0], sizeof(int), (void*)&test[0]);
	sceKernelStartThread(threads[1], sizeof(int), (void*)&test[1]);
	sceKernelStartThread(threads[2], sizeof(int), (void*)&test[2]);
	sceKernelStartThread(threads[3], sizeof(int), (void*)&test[3]);
	sceKernelStartThread(threads[4], sizeof(int), (void*)&test[4]);

	sceKernelDelayThread(10 * 1000);

	printf("---\n");
	PRINT_SEMAPHORE(sema);
	printf("---\n");
	sceKernelSignalSema(sema, 1);
	
	sceKernelDelayThread(10 * 1000);

	printf("---\n");
	PRINT_SEMAPHORE(sema);
	printf("---\n");

	sceKernelSignalSema(sema, 2);
	
	sceKernelDelayThread(10 * 1000);

	printf("---\n");
	PRINT_SEMAPHORE(sema);
	printf("---\n");

	sceKernelDeleteSema(sema);
	printf("\n\n");
}

int main(int argc, char **argv) {
	execPriorityTests(0x000);
	execPriorityTests(0x100);
}