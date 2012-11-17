#include <common.h>

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <psploadexec.h>

#define PRINT_SEMAPHORE(sema) { \
	if (sema > 0) { \
		SceKernelSemaInfo semainfo; \
		int result = sceKernelReferSemaStatus(sema, &semainfo); \
		if (result == 0) { \
			printf("Sema: OK (size=%d,name='%s',attr=%d,init=%d,cur=%d,max=%d,wait=%d)\n", semainfo.size, semainfo.name, semainfo.attr, semainfo.initCount, semainfo.currentCount, semainfo.maxCount, semainfo.numWaitThreads); \
		} else { \
			printf("Sema: Invalid (%08X)\n", result); \
		} \
	} else { \
		printf("Sema: Failed (%08X)\n", sema); \
	} \
}

#define CREATE_PRIORITY_THREAD(func, priority) \
	sceKernelCreateThread(#func, (void*)&func, priority, 0x10000, 0, NULL)
#define CREATE_SIMPLE_THREAD(func) CREATE_PRIORITY_THREAD(func, 0x12)

#define SETUP_SCHED_TEST \
	static int scheduleTestFunc(int argSize, void* argPointer) { \
		printf("B"); \
		sceKernelWaitSemaCB(*(int*) argPointer, 1, NULL); \
		printf("D"); \
		return 0; \
	}

#define TWO_STEP_SCHED_TEST(init1, init2, x, y) { \
	SceUID thread = CREATE_SIMPLE_THREAD(scheduleTestFunc); \
	SceUID sema1 = sceKernelCreateSema("schedTest1", 0, init1, 1, NULL); \
	SceUID sema2 = sceKernelCreateSema("schedTest2", 0, init2, 1, NULL); \
	\
	printf("A"); \
	sceKernelStartThread(thread, sizeof(int), &sema1); \
	x \
	printf("C"); \
	y \
	printf("E\n"); \
	sceKernelDeleteSema(sema1); \
	sceKernelDeleteSema(sema2); \
}
#define BASIC_SCHED_TEST(x) TWO_STEP_SCHED_TEST(0, 1, x, sceKernelDeleteSema(sema1);)

int sceKernelCancelSema(SceUID semaId, int count, int *waitThreads);