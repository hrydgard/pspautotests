// A user module with a few relocations (its module info and lib.ent need them).

#include <pspkernel.h>

#ifdef KERNEL
// Like PMDVIS's kcall.prx.
PSP_MODULE_INFO("cutrelchild", 0x1006, 1, 1);
#else
PSP_MODULE_INFO("cutrelchild", 0, 1, 1);
#endif

static int value = 0x1234;
int *valuePtr = &value;

int module_start(SceSize args, void *argp) {
	return *valuePtr == 0x1234 ? 0 : 1;
}

int childFunc(void) {
	return 0x5678;
}

int module_stop(SceSize args, void *argp) {
	return 0;
}
