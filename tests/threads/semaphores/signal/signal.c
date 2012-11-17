#include "../sub_shared.h"

SETUP_SCHED_TEST;

#define SIGNAL_TEST(title, sema, count) { \
	int result = sceKernelSignalSema(sema, count); \
	if (result == 0) { \
		printf("%s: OK\n", title); \
	} else { \
		printf("%s: Failed (%X)\n", title, result); \
	} \
	PRINT_SEMAPHORE(sema); \
}

int main(int argc, char **argv) {
	SceUID sema = sceKernelCreateSema("signal", 0, 0, 1, NULL);
	PRINT_SEMAPHORE(sema);

	SIGNAL_TEST("Basic +2", sema, 2);
	SIGNAL_TEST("Basic +1", sema, 1);
	SIGNAL_TEST("Negative - 1", sema, -1);
	SIGNAL_TEST("Negative - 2", sema, -2);
	SIGNAL_TEST("Zero", sema, 0);

	sceKernelDeleteSema(sema);

	sema = sceKernelCreateSema("signal", 0, -3, 3, NULL);
	PRINT_SEMAPHORE(sema);
	SIGNAL_TEST("Start negative", sema, 1);
	sceKernelDeleteSema(sema);

	TWO_STEP_SCHED_TEST(0, 0,
		sceKernelSignalSema(sema2, 1);
	,
		sceKernelSignalSema(sema1, 1);
	);

	SIGNAL_TEST("NULL", 0, 1);
	SIGNAL_TEST("Invalid", 0xDEADBEEF, 1);
	SIGNAL_TEST("Deleted", sema, 1);

	return 0;
}