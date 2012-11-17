#include "../sub_shared.h"

SETUP_SCHED_TEST;

#define DELETE_TEST(title, mutex) { \
	int result = sceKernelDeleteMutex(mutex); \
	if (result == 0) { \
		printf("%s: OK\n", title); \
	} else { \
		printf("%s: Failed (%X)\n", title, result); \
	} \
}

int main(int argc, char **argv) {
	DELETE_TEST("NULL", 0);
	DELETE_TEST("Invalid", 0xDEADBEEF);

	SceUID mutex = sceKernelCreateMutex("delete", 0, 0, NULL);
	DELETE_TEST("Valid", mutex);
	DELETE_TEST("Twice", mutex);

	BASIC_SCHED_TEST(
		sceKernelDeleteMutex(mutex2);
	);

	return 0;
}