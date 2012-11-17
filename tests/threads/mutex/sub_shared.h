#include <common.h>

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <psploadexec.h>

#define CREATE_PRIORITY_THREAD(func, priority) \
	sceKernelCreateThread(#func, (void*)&func, priority, 0x10000, 0, NULL)
#define CREATE_SIMPLE_THREAD(func) CREATE_PRIORITY_THREAD(func, 0x12)

#define BASIC_SCHED_TEST(x) { \
	SceUID thread = CREATE_SIMPLE_THREAD(scheduleTestFunc); \
	SceUID mutex1 = sceKernelCreateMutex("schedTest1", 0, 1, NULL); \
	SceUID mutex2 = sceKernelCreateMutex("schedTest2", 0, 0, NULL); \
	\
	printf("Scheduling: A"); \
	sceKernelStartThread(thread, sizeof(int), &mutex1); \
	x \
	printf("C"); \
	/* Needed or sometimes it's out of order, probably an IO buffer issue. */ \
	fflush(stdout); \
	sceKernelDeleteMutex(mutex1); \
	printf("E\n"); \
	sceKernelDeleteMutex(mutex2); \
}

SceUID sceKernelCreateMutex(const char *name, uint attributes, int initial_count, void *options);
int sceKernelDeleteMutex(SceUID mutexId);
int sceKernelLockMutex(SceUID mutexId, int count, SceUInt *timeout);
int sceKernelLockMutexCB(SceUID mutexId, int count, SceUInt *timeout);
int sceKernelTryLockMutex(SceUID mutexId, int count);
int sceKernelUnlockMutex(SceUID mutexId, int count);

int scheduleTestFunc(int argSize, void* argPointer);