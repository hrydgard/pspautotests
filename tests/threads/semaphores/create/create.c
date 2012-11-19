#include "../sub_shared.h"

SETUP_SCHED_TEST;

#define CREATE_TEST(title, name, attr, count, max, options) { \
	sema = sceKernelCreateSema(name, attr, count, max, options); \
	if (sema > 0) { \
		printf("%s: OK\n", title); \
		sceKernelDeleteSema(sema); \
	} else { \
		printf("%s: Failed (%X)\n", title, sema); \
	} \
}

int main(int argc, char **argv) {
	SceUID sema;

	CREATE_TEST("NULL name", NULL, 0, 0, 2, NULL);
	CREATE_TEST("Blank name", "", 0, 0, 2, NULL);
	CREATE_TEST("Long name", "1234567890123456789012345678901234567890123456789012345678901234", 0, 0, 2, NULL);
	CREATE_TEST("Weird attr", "create", 1, 0, 2, NULL);
	CREATE_TEST("Negative initial count", "create", 0, -1, 2, NULL);
	CREATE_TEST("Positive initial count", "create", 0, 1, 2, NULL);
	CREATE_TEST("Initial count above max", "create", 0, 3, 2, NULL);
	CREATE_TEST("Negative max count", "create", 0, 0, -1, NULL);
	CREATE_TEST("Large initial count", "create", 0, 65537, 0, NULL);
	CREATE_TEST("Large max count", "create", 0, 0, 65537, NULL);

	// Two with the same name?
	SceUID sema1 = sceKernelCreateSema("create", 0, 0, 2, NULL);
	SceUID sema2 = sceKernelCreateSema("create", 0, 0, 2, NULL);
	PRINT_SEMAPHORE(sema1);
	PRINT_SEMAPHORE(sema2);
	sceKernelDeleteSema(sema1);
	sceKernelDeleteSema(sema2);

	BASIC_SCHED_TEST("NULL name",
		sema = sceKernelCreateSema(NULL, 0, 0, 1, NULL);
		result = sema > 0 ? : sema;
	);
	BASIC_SCHED_TEST("Create signaled",
		sema = sceKernelCreateSema("create2", 0, 1, 1, NULL);
		result = sema > 0 ? : sema;
	);
	sceKernelDeleteSema(sema);
	BASIC_SCHED_TEST("Create not signaled",
		sema = sceKernelCreateSema("create2", 0, 0, 1, NULL);
		result = sema > 0 ? : sema;
	);
	sceKernelDeleteSema(sema);

	TWO_STEP_SCHED_TEST("Initial not signaled", 0, 1,
		sceKernelDelayThread(1000);
	,
		result = sceKernelSignalSema(sema1, 1);
	);
	TWO_STEP_SCHED_TEST("Initial signaled", 1, 1,
		sceKernelDelayThread(1000);
	,
		result = sceKernelSignalSema(sema1, 1);
	);

	SceUID semas[1024];
	int i, result = 0;
	for (i = 0; i < 1024; i++)
	{
		semas[i] = sceKernelCreateSema("create", 0, 0, 2, NULL);
		if (semas[i] < 0)
		{
			result = semas[i];
			break;
		}
	}

	if (result != 0)
		printf("Create 1024: Failed at %d (%08X)\n", i, result);
	else
		printf("Create 1024: OK\n");

	while (--i >= 0)
		sceKernelDeleteSema(semas[i]);

	return 0;
}