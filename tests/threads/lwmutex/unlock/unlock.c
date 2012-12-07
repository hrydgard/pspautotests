#include "../sub_shared.h"

SETUP_SCHED_TEST;

#define UNLOCK_TEST_SIMPLE(title, workareaPtr, count) { \
	int result = sceKernelUnlockLwMutex(workareaPtr, count); \
	if (result == 0) { \
		printf("%s: OK\n", title); \
	} else { \
		printf("%s: Failed (%X)\n", title, result); \
	} \
}

#define UNLOCK_TEST(title, attr, initial, count) { \
	SceLwMutexWorkarea workarea; \
	int result = sceKernelCreateLwMutex(&workarea, "lock", attr, initial, NULL); \
	if (result == 0) { \
		UNLOCK_TEST_SIMPLE(title, &workarea, count); \
	} else { \
		printf("%s: Failed (%X)\n", title, result); \
	} \
	PRINT_LWMUTEX(workarea); \
	sceKernelDeleteLwMutex(&workarea); \
	FAKE_LWMUTEX(workarea, attr, initial); \
	UNLOCK_TEST_SIMPLE(title " (fake)", &workarea, count); \
	PRINT_LWMUTEX(workarea); \
}

#define UNLOCK_TEST_THREAD(title, attr, initial, count) { \
	printf("%s: ", title); \
	schedulingLogPos = 0; \
	schedulingResult = -1; \
	SceLwMutexWorkarea workarea; \
	sceKernelCreateLwMutex(&workarea, "lock", attr, initial, NULL); \
	void *workareaPtr = &workarea; \
	sceKernelStartThread(lockThread, sizeof(void*), &workareaPtr); \
	sceKernelDelayThread(400); \
	int result = sceKernelUnlockLwMutex(&workarea, count); \
	schedulingLogPos += sprintf(schedulingLog + schedulingLogPos, "L2 "); \
	sceKernelDelayThread(600); \
	sceKernelDeleteLwMutex(&workarea); \
	sceKernelWaitThreadEnd(lockThread, NULL); \
	schedulingLog[schedulingLogPos] = 0; \
	schedulingLogPos = 0; \
	if (result == 0) { \
		printf("%sOK (thread=%08X)\n", schedulingLog, schedulingResult); \
	} else { \
		printf("%sFailed (thread=%08X, main=%08X)\n", schedulingLog, schedulingResult, result); \
	} \
	sceKernelTerminateThread(lockThread); \
	\
	FAKE_LWMUTEX(workarea, attr, initial); \
	printf("%s (fake): ", title); \
	schedulingLogPos = 0; \
	schedulingResult = -1; \
	sceKernelStartThread(lockThread, sizeof(void*), &workareaPtr); \
	sceKernelDelayThread(400); \
	result = sceKernelUnlockLwMutex(&workarea, count); \
	schedulingLogPos += sprintf(schedulingLog + schedulingLogPos, "L2 "); \
	sceKernelDelayThread(600); \
	sceKernelWaitThreadEnd(lockThread, NULL); \
	if (result == 0) { \
		printf("%sOK (thread=%08X)\n", schedulingLog, schedulingResult); \
	} else { \
		printf("%sFailed (thread=%08X, main=%08X)\n", schedulingLog, schedulingResult, result); \
	} \
	sceKernelTerminateThread(lockThread); \
}

static int lockFunc(SceSize argSize, void* argPointer) {
	SceUInt timeout = 1000;
	schedulingResult = sceKernelLockLwMutex(*(void**) argPointer, 1, &timeout);
	schedulingLogPos += sprintf(schedulingLog + schedulingLogPos, "L1 "); \
	sceKernelDelayThread(1000);
	if (schedulingResult == 0)
		sceKernelUnlockLwMutex(*(void**) argPointer, 1);
	return 0;
}

static int unlockFunc(SceSize argSize, void* argPointer) {
	int result = sceKernelUnlockLwMutex(*(void**) argPointer, 1);
	printf("After unlock: %08X\n", result);
	return 0;
}

int main(int argc, char **argv) {
	UNLOCK_TEST("Unlock 0 => 0", PSP_MUTEX_ATTR_FIFO, 0, 0);
	UNLOCK_TEST("Unlock 0 => 1", PSP_MUTEX_ATTR_FIFO, 0, 1);
	UNLOCK_TEST("Unlock 0 => 2", PSP_MUTEX_ATTR_FIFO, 0, 2);
	UNLOCK_TEST("Unlock 0 => -1", PSP_MUTEX_ATTR_FIFO, 0, -1);
	UNLOCK_TEST("Unlock 1 => 1", PSP_MUTEX_ATTR_FIFO, 1, 1);
	UNLOCK_TEST("Unlock 1 => 2", PSP_MUTEX_ATTR_FIFO, 1, 2);
	UNLOCK_TEST("Unlock 2 => 1", PSP_MUTEX_ATTR_FIFO, 2, 1);
	UNLOCK_TEST("Unlock 0 => 2 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 0, 2);
	UNLOCK_TEST("Unlock 0 => -1 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 0, -1);
	UNLOCK_TEST("Unlock 1 => 1 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 1, 1);
	UNLOCK_TEST("Unlock 1 => 2 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 1, 2);

	SceUID lockThread = CREATE_SIMPLE_THREAD(lockFunc);
	UNLOCK_TEST_THREAD("Locked 1 => 1", PSP_MUTEX_ATTR_FIFO, 1, 1);
	UNLOCK_TEST_THREAD("Locked 0 => 1", PSP_MUTEX_ATTR_FIFO, 0, 1);
	UNLOCK_TEST_THREAD("Locked 1 => 1 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 1, 1);
	UNLOCK_TEST_THREAD("Locked 2 => 1 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 2, 1);
	UNLOCK_TEST_THREAD("Locked 1 => 2 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 1, 2);
	UNLOCK_TEST_THREAD("Locked 2 => 2 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 2, 2);
	UNLOCK_TEST_THREAD("Locked 0 => 1 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 0, 1);

	SceUID unlockThread = CREATE_SIMPLE_THREAD(unlockFunc);
	SceLwMutexWorkarea workarea;
	sceKernelCreateLwMutex(&workarea, "unlock", 0, 1, NULL);
	void *workareaPtr = &workarea;
	sceKernelStartThread(unlockThread, sizeof(void*), &workareaPtr);
	sceKernelDelayThread(500);
	sceKernelDeleteLwMutex(&workarea);

	// Crashes.
	//UNLOCK_TEST_SIMPLE("NULL => 0", 0, 0);
	//UNLOCK_TEST_SIMPLE("NULL => 1", 0, 1);
	//UNLOCK_TEST_SIMPLE("Invalid => 1", 0xDEADBEEF, 1);
	UNLOCK_TEST_SIMPLE("Deleted => 1", &workarea, 1);

	BASIC_SCHED_TEST("Zero",
		result = sceKernelUnlockLwMutex(&workarea2, 0);
	);
	BASIC_SCHED_TEST("Unlock same",
		result = sceKernelUnlockLwMutex(&workarea1, 1);
	);
	BASIC_SCHED_TEST("Unlock other",
		result = sceKernelUnlockLwMutex(&workarea2, 1);
	);

	return 0;
}