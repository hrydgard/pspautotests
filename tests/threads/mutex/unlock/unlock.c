#include "../sub_shared.h"

SETUP_SCHED_TEST;

#define UNLOCK_TEST_SIMPLE(title, mutex, count) { \
	int result = sceKernelUnlockMutex(mutex, count); \
	if (result == 0) { \
		printf("%s: OK\n", title); \
	} else { \
		printf("%s: Failed (%X)\n", title, result); \
	} \
}

#define UNLOCK_TEST(title, attr, initial, count) { \
	SceUID mutex = sceKernelCreateMutex("lock", attr, initial, NULL); \
	UNLOCK_TEST_SIMPLE(title, mutex, count); \
	sceKernelDeleteMutex(mutex); \
}

#define UNLOCK_TEST_THREAD(title, attr, initial, count) { \
	printf("%s: ", title); \
	SceUID mutex = sceKernelCreateMutex("lock", attr, initial, NULL); \
	sceKernelStartThread(lockThread, sizeof(int), &mutex); \
	sceKernelDelayThread(500); \
	int result = sceKernelUnlockMutex(mutex, count); \
	printf("L2 "); \
	sceKernelDelayThread(500); \
	if (result == 0) { \
		printf("OK\n"); \
	} else { \
		printf("Failed (%X)\n", result); \
	} \
	sceKernelDeleteMutex(mutex); \
	sceKernelTerminateThread(lockThread); \
}

static int lockFunc(SceSize argSize, void* argPointer) {
	SceUInt timeout = 1000;
	sceKernelLockMutex(*(int*) argPointer, 1, &timeout);
	printf("L1 ");
	sceKernelDelayThread(1000);
	return 0;
}

static int deleteMeFunc(SceSize argSize, void* argPointer) {
	sceKernelLockMutex(*(int*) argPointer, 1, NULL);
	sceKernelDelayThread(1000);
	int result = sceKernelUnlockMutex(*(int*) argPointer, 1);
	printf("After delete: %08X\n", result);
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

	SceUID deleteThread = CREATE_SIMPLE_THREAD(deleteMeFunc);
	SceUID mutex = sceKernelCreateMutex("unlock", 0, 0, NULL);
	sceKernelStartThread(deleteThread, sizeof(int), &mutex);
	sceKernelDelayThread(500);
	sceKernelDeleteMutex(mutex);

	UNLOCK_TEST_SIMPLE("NULL => 0", 0, 0);
	UNLOCK_TEST_SIMPLE("NULL => 1", 0, 1);
	UNLOCK_TEST_SIMPLE("Invalid => 1", 0xDEADBEEF, 1);
	UNLOCK_TEST_SIMPLE("Deleted => 1", mutex, 1);

	BASIC_SCHED_TEST("NULL",
		result = sceKernelUnlockMutex(0, 1);
	);
	BASIC_SCHED_TEST("Zero",
		result = sceKernelUnlockMutex(mutex2, 0);
	);
	BASIC_SCHED_TEST("Unlock same",
		result = sceKernelUnlockMutex(mutex1, 1);
	);
	BASIC_SCHED_TEST("Unlock other",
		result = sceKernelUnlockMutex(mutex2, 1);
	);

	return 0;
}