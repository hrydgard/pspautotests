#include "../sub_shared.h"

SETUP_SCHED_TEST;

#define CANCEL_TEST(title, sema, count) { \
	int result = sceKernelCancelSema(sema, count, NULL); \
	if (result == 0) { \
		printf("%s: OK\n", title); \
	} else { \
		printf("%s: Failed (%X)\n", title, result); \
	} \
	PRINT_SEMAPHORE(sema); \
}

#define CANCEL_TEST_WITH_WAIT(title, sema, count) { \
	int waitThreads = 99; \
	int result = sceKernelCancelSema(sema, count, &waitThreads); \
	if (result == 0) { \
		printf("%s: OK (%d waiting)\n", title, waitThreads); \
	} else { \
		printf("%s: Failed (%X, %d waiting)\n", title, result, waitThreads); \
	} \
	PRINT_SEMAPHORE(sema); \
}

int main(int argc, char **argv) {
	SceUID sema = sceKernelCreateSema("cancel", 0, 0, 1, NULL);

	CANCEL_TEST("Normal", sema, 1);
	CANCEL_TEST("Greater than max", sema, 3);
	CANCEL_TEST("Zero", sema, 0);
	CANCEL_TEST("Negative -3", sema, -3);
	CANCEL_TEST("Negative -1", sema, -1);

	CANCEL_TEST_WITH_WAIT("Normal", sema, 1);
	CANCEL_TEST_WITH_WAIT("Greater than max", sema, 3);
	CANCEL_TEST_WITH_WAIT("Zero", sema, 0);
	CANCEL_TEST_WITH_WAIT("Negative -3", sema, -3);
	CANCEL_TEST_WITH_WAIT("Negative -1", sema, -1);

	sceKernelDeleteSema(sema);

	TWO_STEP_SCHED_TEST(0, 1,
		CANCEL_TEST_WITH_WAIT("To 0", sema1, 0);
	,
		CANCEL_TEST_WITH_WAIT("To 1", sema1, 1);
	);

	CANCEL_TEST("NULL", 0, 0);
	CANCEL_TEST("Invalid", 0xDEADBEEF, 0);
	CANCEL_TEST("Deleted", sema, 0);

	return 0;
}