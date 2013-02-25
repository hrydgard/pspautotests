#include "shared.h"
#include <limits.h>

#define CREATE_TEST(title, workarea, name, attr, count, options) { \
	int result = sceKernelCreateLwMutex(workarea, name, attr, count, options); \
	if (result == 0) { \
		printf("%s: OK ", title); \
		printfLwMutexWorkarea(workarea); \
		sceKernelDeleteLwMutex(workarea); \
	} else { \
		printf("%s: Failed (%X)\n", title, result); \
	} \
}

int main(int argc, char **argv) {
	SceLwMutexWorkarea workarea, workarea1, workarea2;
	int result1, result2;

	// Crash.
	//CREATE_TEST("NULL workarea", NULL, "test", 0, 0, NULL);
	CREATE_TEST("NULL name", &workarea, NULL, 0, 0, NULL);
	CREATE_TEST("Blank name", &workarea, "", 0, 0, NULL);
	CREATE_TEST("Long name", &workarea, "1234567890123456789012345678901234567890123456789012345678901234", 0, 0, NULL);
	CREATE_TEST("Weird attr", &workarea, "create", 1, 0, NULL);
	CREATE_TEST("0x100 attr", &workarea, "create", 0x100, 0, NULL);
	CREATE_TEST("0x200 attr", &workarea, "create", 0x200, 0, NULL);
	CREATE_TEST("0x300 attr", &workarea, "create", 0x300, 0, NULL);
	CREATE_TEST("0x3FF attr", &workarea, "create", 0x3FF, 0, NULL);
	CREATE_TEST("0x400 attr", &workarea, "create", 0x400, 0, NULL);
	CREATE_TEST("Positive count", &workarea, "create", 0, 1, NULL);
	CREATE_TEST("Negative count", &workarea, "create", 0, -1, NULL);
	CREATE_TEST("Count = -5", &workarea, "create", 0, -5, NULL);
	CREATE_TEST("Count = 2", &workarea, "create", 0, 2, NULL);
	CREATE_TEST("Negative count (recursive)", &workarea, "create", 0x200, -1, NULL);
	CREATE_TEST("Count = -5 (recursive)", &workarea, "create", 0x200, -5, NULL);
	CREATE_TEST("Count = 2 (recursive)", &workarea, "create", 0x200, 2, NULL);
	CREATE_TEST("Medium count (recursive)", &workarea, "create", 0x200, 255, NULL);
	CREATE_TEST("Large count (recursive)", &workarea, "create", 0x200, 65537, NULL);

	result1 = sceKernelCreateLwMutex(&workarea1, "create", 0, 0, NULL);
	result2 = sceKernelCreateLwMutex(&workarea2, "create", 0, 0, NULL);
	if (result1 == 0 && result2 == 0) {
		printf("Two with same name: OK\n");
	} else {
		printf("Two with same name: Failed (%X, %X)\n", result1, result2);
	}
	sceKernelDeleteLwMutex(&workarea1);
	sceKernelDeleteLwMutex(&workarea2);

	sceKernelCreateLwMutex(&workarea, "create1", 0, 0, NULL);
	CREATE_TEST("Two with same workarea", &workarea, "create2", 0, 0, NULL);
	// Leaks.

	BASIC_SCHED_TEST("NULL name",
		result = sceKernelCreateLwMutex(&workarea, NULL, 0, 0, NULL);
	);
	BASIC_SCHED_TEST("Negative count",
		result = sceKernelCreateLwMutex(&workarea, "create2", 0, -1, NULL);
	);
	BASIC_SCHED_TEST("Create locked",
		result = sceKernelCreateLwMutex(&workarea, "create2", 0, 1, NULL);
	);
	sceKernelDeleteLwMutex(&workarea);
	BASIC_SCHED_TEST("Create not locked",
		result = sceKernelCreateLwMutex(&workarea, "create2", 0, 0, NULL);
	);
	sceKernelDeleteLwMutex(&workarea);

	LOCKED_SCHED_TEST("Initial not locked", 0, 0,
		sceKernelDelayThread(1000);
		result = 0;
	);

	LOCKED_SCHED_TEST("Initial locked", 1, 0,
		sceKernelDelayThread(1000);
		result = 0;
	);

	SceLwMutexWorkarea workareas[1024];
	int i, result = 0;
	for (i = 0; i < 1024; i++)
	{
		result = sceKernelCreateLwMutex(&workareas[i], "create", 0, 0, NULL);
		if (result != 0)
			break;
	}

	if (result != 0)
		printf("Create 1024: Failed at %d (%08X)\n", i, result);
	else
		printf("Create 1024: OK\n");

	while (--i >= 0)
		sceKernelDeleteLwMutex(&workareas[i]);

	return 0;
}