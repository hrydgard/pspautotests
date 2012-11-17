#include "../sub_shared.h"

SETUP_SCHED_TEST;

#define REFER_TEST(title, sema, semainfo) { \
	int result = sceKernelReferSemaStatus(sema, semainfo); \
	if (result == 0) { \
		printf("%s: OK\n", title); \
	} else { \
		printf("%s: Failed (%X)\n", title, result); \
	} \
}

int main(int argc, char **argv) {
	SceUID sema = sceKernelCreateSema("refer1", 0, 0, 1, NULL);
	SceKernelSemaInfo semainfo;

	// Crashes.
	//REFER_TEST("NULL info", sema, NULL);
	REFER_TEST("Normal", sema, &semainfo);

	sceKernelDeleteSema(sema);

	REFER_TEST("NULL", 0, &semainfo);
	REFER_TEST("Invalid", 0xDEADBEEF, &semainfo);
	REFER_TEST("Deleted", sema, &semainfo);

	BASIC_SCHED_TEST(
		sceKernelReferSemaStatus(sema2, &semainfo);
	);

	return 0;
}