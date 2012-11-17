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

	// NULL name argument.
	sema = sceKernelCreateSema(NULL, 0, 0, 2, NULL);
	PRINT_SEMAPHORE(sema);
	sceKernelDeleteSema(sema);

	// Blank name argument.
	sema = sceKernelCreateSema("", 0, 0, 2, NULL);
	PRINT_SEMAPHORE(sema);
	sceKernelDeleteSema(sema);

	// Long name argument.
	sema = sceKernelCreateSema("1234567890123456789012345678901234567890123456789012345678901234", 0, 0, 2, NULL);
	PRINT_SEMAPHORE(sema);
	sceKernelDeleteSema(sema);

	// Weird attr value (1).
	sema = sceKernelCreateSema("create", 1, 0, 2, NULL);
	PRINT_SEMAPHORE(sema);
	sceKernelDeleteSema(sema);

	// Negative initial count.
	sema = sceKernelCreateSema("create", 0, -1, 2, NULL);
	PRINT_SEMAPHORE(sema);
	sceKernelDeleteSema(sema);

	// Positive initial count.
	sema = sceKernelCreateSema("create", 0, 1, 2, NULL);
	PRINT_SEMAPHORE(sema);
	sceKernelDeleteSema(sema);

	// Initial count above max.
	sema = sceKernelCreateSema("create", 0, 3, 2, NULL);
	PRINT_SEMAPHORE(sema);
	sceKernelDeleteSema(sema);

	// Negative max count.
	sema = sceKernelCreateSema("create", 0, 0, -1, NULL);
	PRINT_SEMAPHORE(sema);
	sceKernelDeleteSema(sema);

	// Large initial count.
	sema = sceKernelCreateSema("create", 0, 65537, 0, NULL);
	PRINT_SEMAPHORE(sema);
	sceKernelDeleteSema(sema);

	// Large max count.
	sema = sceKernelCreateSema("create", 0, 0, 65537, NULL);
	PRINT_SEMAPHORE(sema);
	sceKernelDeleteSema(sema);

	// Two with the same name?
	SceUID sema1 = sceKernelCreateSema("create", 0, 0, 2, NULL);
	SceUID sema2 = sceKernelCreateSema("create", 0, 0, 2, NULL);
	PRINT_SEMAPHORE(sema1);
	PRINT_SEMAPHORE(sema2);
	sceKernelDeleteSema(sema1);
	sceKernelDeleteSema(sema2);

	// Verify scheduling order.
	printf("A");
	sema1 = sceKernelCreateSema("create1", 0, 0, 1, NULL);
	SceUID thread = sceKernelCreateThread("createTest", (void *)&threadFunc, 0x12, 0x10000, 0, NULL);
	sceKernelStartThread(thread, sizeof(int), &sema1);
	sema2 = sceKernelCreateSema("create2", 0, 0, 1, NULL);
	printf("C");
	// TODO: Does create also resched like this?
	sceKernelDeleteSema(sema1);
	printf("E\n");
	sceKernelDeleteSema(sema2);

	// Note: causes PSP to be unstable.
	/*SceUID create_max[2048];
	int i;
	for (i = 0; i < 2048; i++)
	{
		create_max[i] = sceKernelCreateSema("create", 0, 0, 2, NULL);
		if (create_max[i] < 0)
		{
			PRINT_SEMAPHORE(create_max[i]);
			break;
		}
	}
	printf("Created %d successfully.\n", i);
	int j;
	for (j = 0; j < i; j++)
		sceKernelDeleteSema(create_max[i]);*/
}