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
	SceUID sema;

	sema = sceKernelCreateSema("poll1", 0, 1, 1, NULL);
	printf("%08X\n", sceKernelPollSema(sema, 1));
	printf("%08X\n", sceKernelPollSema(sema, 1));
	printf("%08X\n", sceKernelPollSema(sema, 0));
	sceKernelSignalSema(sema, 1);
	printf("%08X\n", sceKernelPollSema(sema, 0));
	printf("%08X\n", sceKernelPollSema(sema, -1));
	sceKernelDeleteSema(sema);

	// Poll NULL?
	printf("%08X\n", sceKernelPollSema(0, 1));

	// Poll invalid?
	printf("%08X\n", sceKernelPollSema(sema, 1));

	SceUID sema1 = sceKernelCreateSema("poll1", 0, 0, 1, NULL);
	SceUID sema2 = sceKernelCreateSema("poll2", 0, 0, 1, NULL);

	// Verify scheduling order.
	printf("A");
	SceUID thread = sceKernelCreateThread("pollTest", (void *)&threadFunc, 0x12, 0x10000, 0, NULL);
	sceKernelStartThread(thread, sizeof(int), &sema1);
	sceKernelPollSema(sema2, 1);
	printf("C");
	sceKernelDeleteSema(sema1);
	printf("E\n");
}