#include <common.h>

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <psploadexec.h>

#define PSP_MUTEX_ATTR_FIFO 0
#define PSP_MUTEX_ATTR_PRIORITY 0x100
#define PSP_MUTEX_ATTR_ALLOW_RECURSIVE 0x200

#define CREATE_PRIORITY_THREAD(func, priority) \
	sceKernelCreateThread(#func, &func, priority, 0x10000, 0, NULL)
#define CREATE_SIMPLE_THREAD(func) CREATE_PRIORITY_THREAD(func, 0x12)

// Log a checkpoint and which thread was last active.
#define SCHED_LOG(letter, placement) { \
	int old = schedulingPlacement; \
	schedulingPlacement = placement; \
	printf(#letter "%d", old); \
}

// Avoid linking or other things.
#define SETUP_SCHED_TEST \
	/* Keep track of the last thread we saw here. */ \
	static volatile int schedulingPlacement = 0; \
	/* So we can log the result from the thread. */ \
	static int schedulingResult = -1; \
	\
	static int scheduleTestFunc(SceSize argSize, void* argPointer) { \
		int result = 0x800201A8; \
		SceUInt timeout; \
		schedulingResult = -1; \
		\
		SCHED_LOG(B, 2); \
		/* Constantly loop setting the placement to 2 whenever we're active. */ \
		while (result == 0x800201A8) { \
			schedulingPlacement = 2; \
			timeout = 1; \
			result = sceKernelLockMutexCB(*(int*) argPointer, 1, &timeout); \
		} \
		SCHED_LOG(D, 2); \
		\
		schedulingResult = result; \
		return 0; \
	}

#define LOCKED_SCHED_TEST(title, init1, init2, x) { \
	SceUID thread = CREATE_SIMPLE_THREAD(scheduleTestFunc); \
	SceUID mutex1 = sceKernelCreateMutex("schedTest1", 0, init1, NULL); \
	SceUID mutex2 = sceKernelCreateMutex("schedTest2", 0, init2, NULL); \
	int result = -1; \
	\
	schedulingPlacement = 1; \
	printf("%s: ", title); \
	\
	SCHED_LOG(A, 1); \
	sceKernelStartThread(thread, sizeof(int), &mutex1); \
	SCHED_LOG(C, 1); \
	x \
	SCHED_LOG(E, 1); \
	sceKernelDeleteMutex(mutex1); \
	SCHED_LOG(F, 1); \
	\
	printf(" (thread=%08X, main=%08X)\n", schedulingResult, result); \
	sceKernelDeleteMutex(mutex2); \
}
#define BASIC_SCHED_TEST(title, x) LOCKED_SCHED_TEST(title, 1, 0, x);

SceUID sceKernelCreateMutex(const char *name, uint attributes, int initial_count, void *options);
int sceKernelDeleteMutex(SceUID mutexId);
int sceKernelLockMutex(SceUID mutexId, int count, SceUInt *timeout);
int sceKernelLockMutexCB(SceUID mutexId, int count, SceUInt *timeout);
int sceKernelTryLockMutex(SceUID mutexId, int count);
int sceKernelUnlockMutex(SceUID mutexId, int count);