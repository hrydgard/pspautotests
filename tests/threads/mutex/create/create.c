#include "../sub_shared.h"

SETUP_SCHED_TEST;

#define CREATE_TEST(title, name, attr, count, options) { \
	mutex = sceKernelCreateMutex(name, attr, count, options); \
	if (mutex > 0) { \
		printf("%s: OK\n", title); \
		sceKernelDeleteMutex(mutex); \
	} else { \
		printf("%s: Failed (%X)\n", title, mutex); \
	} \
}

static int unlockedTestFunc(int argSize, void* argPointer) {
	printf("B");
	sceKernelLockMutexCB(*(int*) argPointer, 1, NULL);
	printf("C");
	return 0;
}

int main(int argc, char **argv) {
	SceUID mutex;

	CREATE_TEST("NULL name", NULL, 0, 0, NULL);
	CREATE_TEST("Blank name", "", 0, 0, NULL);
	CREATE_TEST("Long name", "1234567890123456789012345678901234567890123456789012345678901234", 0, 0, NULL);
	CREATE_TEST("Weird attr", "create", 1, 0, NULL);
	CREATE_TEST("Negative count", "create", 0, -1, NULL);
	CREATE_TEST("Positive count", "create", 0, 1, NULL);
	CREATE_TEST("Large count", "create", 0, 65537, NULL);

	SceUID mutex1 = sceKernelCreateMutex("create", 0, 0, NULL);
	SceUID mutex2 = sceKernelCreateMutex("create", 0, 0, NULL);
	if (mutex1 > 0 && mutex2 > 0) {
		printf("Two with same name: OK\n");
	} else {
		printf("Two with same name: Failed (%X, %X)\n", mutex1, mutex2);
	}
	sceKernelDeleteMutex(mutex1);
	sceKernelDeleteMutex(mutex2);

	SceUID thread = CREATE_SIMPLE_THREAD(scheduleTestFunc);
	SceUID unlockedThread = CREATE_SIMPLE_THREAD(unlockedTestFunc);

	printf("Scheduling: A");
	mutex1 = sceKernelCreateMutex("create1", 0, 1, NULL);
	sceKernelStartThread(thread, sizeof(int), &mutex1);
	mutex2 = sceKernelCreateMutex("create2", 0, 1, NULL);
	printf("C");
	sceKernelDeleteMutex(mutex1);
	printf("E\n");
	sceKernelDeleteMutex(mutex2);

	printf("Initial locked: A");
	mutex1 = sceKernelCreateMutex("create1", 0, 1, NULL);
	sceKernelStartThread(thread, sizeof(int), &mutex1);
	sceKernelDelayThread(1000);
	printf("C");
	sceKernelUnlockMutex(mutex1, 1);
	printf("E\n");
	sceKernelDeleteMutex(mutex1);

	printf("Initial unlocked: A");
	mutex1 = sceKernelCreateMutex("create1", 0, 0, NULL);
	sceKernelStartThread(unlockedThread, sizeof(int), &mutex1);
	sceKernelDelayThread(1000);
	printf("D");
	sceKernelUnlockMutex(mutex1, 1);
	printf("E\n");
	sceKernelDeleteMutex(mutex1);

	return 0;
}