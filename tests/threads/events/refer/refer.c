#include "../sub_shared.h"

SETUP_SCHED_TEST;

#define REFER_TEST(title, flag, flaginfo) { \
	int result = sceKernelReferEventFlagStatus(flag, flaginfo); \
	if (result == 0) { \
		printf("%s: OK\n", title); \
	} else { \
		printf("%s: Failed (%X)\n", title, result); \
	} \
}

int main(int argc, char **argv) {
	SceUID flag = sceKernelCreateEventFlag("refer1", 0, 0, NULL);
	SceKernelEventFlagInfo flaginfo;

	// Crashes.
	//REFER_TEST("NULL info", flag, NULL);
	REFER_TEST("Normal", flag, &flaginfo);

	sceKernelDeleteEventFlag(flag);

	REFER_TEST("NULL", 0, &flaginfo);
	REFER_TEST("Invalid", 0xDEADBEEF, &flaginfo);
	REFER_TEST("Deleted", flag, &flaginfo);

	BASIC_SCHED_TEST("NULL",
		result = sceKernelReferEventFlagStatus(NULL, &flaginfo);
	);
	BASIC_SCHED_TEST("Refer other",
		result = sceKernelReferEventFlagStatus(flag2, &flaginfo);
	);
	BASIC_SCHED_TEST("Refer same",
		result = sceKernelReferEventFlagStatus(flag1, &flaginfo);
	);

	return 0;
}