// A user module whose module_start and module_stop record the thread they run on.
// The loader passes a pointer to its Results as the argument.

#include <pspkernel.h>
#include <pspthreadman.h>

#include "shared.h"

PSP_MODULE_INFO("startoptionschild", 0, 1, 1);

static void record(ThreadRecord *rec) {
	SceKernelThreadInfo info;
	info.size = sizeof(info);
	if (sceKernelReferThreadStatus(sceKernelGetThreadId(), &info) < 0) {
		rec->called = -1;
		return;
	}
	rec->called = 1;
	rec->stackSize = info.stackSize;
	rec->attr = info.attr;
	rec->priority = info.currentPriority;
	rec->stack = (unsigned int)info.stack;
}

int module_start(SceSize args, void *argp) {
	if (args == sizeof(Results *)) {
		record(&(*(Results **)argp)->start);
	}
	return 0;
}

int module_stop(SceSize args, void *argp) {
	if (args == sizeof(Results *)) {
		record(&(*(Results **)argp)->stop);
	}
	return 0;
}
