#include "shared.h"

inline void createTest(const char *title, const char *name, u32 attr, int count, void *options) {
	SceUID mutex = sceKernelCreateMutex(name, attr, count, options);
	if (mutex > 0) {
		printf("%s: ", title);
		printfMutex(mutex);
		sceKernelDeleteMutex(mutex);
	} else {
		printf("%s: Failed (%X)\n", title, mutex);
	}
}

int main(int argc, char **argv) {
	SceUID mutex;

	createTest("NULL name", NULL, 0, 0, NULL);
	createTest("Blank name", "", 0, 0, NULL);
	createTest("Long name", "1234567890123456789012345678901234567890123456789012345678901234", 0, 0, NULL);
	createTest("Weird attr", "create", 1, 0, NULL);
	createTest("0x100 attr", "create", 0x100, 0, NULL);
	createTest("0x200 attr", "create", 0x200, 0, NULL);
	createTest("0xB00 attr", "create", 0xB00, 0, NULL);
	createTest("0xBFF attr", "create", 0xBFF, 0, NULL);
	createTest("0xC00 attr", "create", 0xC00, 0, NULL);
	createTest("Negative count", "create", 0, -1, NULL);
	createTest("Positive count", "create", 0, 1, NULL);
	createTest("Large count", "create", 0, 65537, NULL);

	SceUID mutex1 = sceKernelCreateMutex("create", 0, 0, NULL);
	SceUID mutex2 = sceKernelCreateMutex("create", 0, 0, NULL);
	if (mutex1 > 0 && mutex2 > 0) {
		printf("Two with same name: OK\n");
	} else {
		printf("Two with same name: Failed (%X, %X)\n", mutex1, mutex2);
	}
	sceKernelDeleteMutex(mutex1);
	sceKernelDeleteMutex(mutex2);

	BASIC_SCHED_TEST("NULL name",
		mutex = sceKernelCreateMutex(NULL, 0, 0, NULL);
		result = mutex > 0 ? 1 : mutex;
	);
	BASIC_SCHED_TEST("Create locked",
		mutex = sceKernelCreateMutex("create2", 0, 1, NULL);
		result = mutex > 0 ? 1 : mutex;
	);
	sceKernelDeleteMutex(mutex);
	BASIC_SCHED_TEST("Create not locked",
		mutex = sceKernelCreateMutex("create2", 0, 0, NULL);
		result = mutex > 0 ? 1 : mutex;
	);
	sceKernelDeleteMutex(mutex);

	LOCKED_SCHED_TEST("Initial not locked", 0, 0,
		sceKernelDelayThread(1000);
		result = 0;
	);

	LOCKED_SCHED_TEST("Initial locked", 1, 0,
		sceKernelDelayThread(1000);
		result = 0;
	);

	SceUID mutexes[1024];
	int i, result = 0;
	for (i = 0; i < 1024; i++)
	{
		mutexes[i] = sceKernelCreateMutex("create", 0, 0, NULL);
		if (mutexes[i] < 0)
		{
			result = mutexes[i];
			break;
		}
	}

	if (result != 0)
		printf("Create 1024: Failed at %d (%08X)\n", i, result);
	else
		printf("Create 1024: OK\n");

	while (--i >= 0)
		sceKernelDeleteMutex(mutexes[i]);

	return 0;
}