#include "../sub_shared.h"

SETUP_SCHED_TEST;

#define DELETE_TEST(title, sema) { \
	int result = sceKernelDeleteSema(sema); \
	if (result == 0) { \
		printf("%s: OK\n", title); \
	} else { \
		printf("%s: Failed (%X)\n", title, result); \
	} \
}

int main(int argc, char **argv) {
	// Verify scheduling order.
	SceUID sema = sceKernelCreateSema("delete1", 0, 0, 1, NULL);

	DELETE_TEST("Normal", sema);
	DELETE_TEST("NULL", 0);
	DELETE_TEST("Invalid", 0xDEADBEEF);
	DELETE_TEST("Deleted", sema);
	
	BASIC_SCHED_TEST(
		sceKernelDeleteSema(sema2);
	);

	return 0;
	
}