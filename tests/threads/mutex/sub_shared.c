#include "sub_shared.h"

int scheduleTestFunc(int argSize, void* argPointer) {
	printf("B");
	// Needed or sometimes it's out of order, probably an IO buffer issue.
	fflush(stdout);
	sceKernelLockMutexCB(*(int*) argPointer, 1, NULL);
	printf("D");
	return 0;
}