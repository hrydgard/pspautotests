#include "../sub_shared.h"

SETUP_SCHED_TEST;

#define LOCK_TEST_SIMPLE(title, mutex, count) { \
	int result = sceKernelTryLockMutex(mutex, count); \
	if (result == 0) { \
		printf("%s: OK\n", title); \
	} else { \
		printf("%s: Failed (%X)\n", title, result); \
	} \
}

#define LOCK_TEST(title, attr, initial, count) { \
	SceUID mutex = sceKernelCreateMutex("lock", attr, initial, NULL); \
	LOCK_TEST_SIMPLE(title, mutex, count); \
	sceKernelDeleteMutex(mutex); \
}

#define LOCK_TEST_THREAD(title, attr, initial, count) { \
	printf("%s: ", title); \
	SceUID mutex = sceKernelCreateMutex("lock", attr, initial, NULL); \
	sceKernelStartThread(lockThread, sizeof(int), &mutex); \
	sceKernelDelayThread(1000); \
	int result = sceKernelTryLockMutex(mutex, count); \
	printf("L2 "); \
	if (result == 0) { \
		printf("OK\n"); \
	} else { \
		printf("Failed (%X)\n", result); \
	} \
	sceKernelDeleteMutex(mutex); \
	sceKernelTerminateThread(lockThread); \
}

static int lockFunc(int argSize, void* argPointer) {
	SceUInt timeout = 100;
	sceKernelLockMutex(*(int*) argPointer, 1, &timeout);
	printf("L1 ");
	sceKernelDelayThread(1100);
	return 0;
}

static int deleteMeFunc(int argSize, void* argPointer) {
	int result = sceKernelTryLockMutex(*(int*) argPointer, 1);
	printf("After delete: %08X\n", result);
	return 0;
}

int main(int argc, char **argv) {
	LOCK_TEST("Lock 0 => 0", PSP_MUTEX_ATTR_FIFO, 0, 0);
	LOCK_TEST("Lock 0 => 1", PSP_MUTEX_ATTR_FIFO, 0, 1);
	LOCK_TEST("Lock 0 => 2", PSP_MUTEX_ATTR_FIFO, 0, 2);
	LOCK_TEST("Lock 0 => -1", PSP_MUTEX_ATTR_FIFO, 0, -1);
	LOCK_TEST("Lock 1 => 1", PSP_MUTEX_ATTR_FIFO, 1, 1);
	LOCK_TEST("Lock 0 => 2 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 0, 2);
	LOCK_TEST("Lock 0 => -1 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 0, -1);
	LOCK_TEST("Lock 1 => 1 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 1, 1);

	SceUID lockThread = CREATE_SIMPLE_THREAD(lockFunc);
	LOCK_TEST_THREAD("Locked 1 => 1", PSP_MUTEX_ATTR_FIFO, 1, 1);
	LOCK_TEST_THREAD("Locked 0 => 1", PSP_MUTEX_ATTR_FIFO, 0, 1);
	LOCK_TEST_THREAD("Locked 1 => 1 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 1, 1);
	LOCK_TEST_THREAD("Locked 0 => 1 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 0, 1);

	// Probably we can't manage to delete it at the same time.
	SceUID deleteThread = CREATE_SIMPLE_THREAD(deleteMeFunc);
	SceUID mutex = sceKernelCreateMutex("lock", 0, 1, NULL);
	sceKernelStartThread(deleteThread, sizeof(int), &mutex);
	sceKernelDeleteMutex(mutex);

	LOCK_TEST_SIMPLE("NULL => 0", 0, 0);
	LOCK_TEST_SIMPLE("NULL => 1", 0, 1);
	LOCK_TEST_SIMPLE("Invalid => 1", 0xDEADBEEF, 1);
	LOCK_TEST_SIMPLE("Deleted => 1", mutex, 1);

	BASIC_SCHED_TEST(
		sceKernelTryLockMutex(mutex2, 1);
	);

	return 0;
}