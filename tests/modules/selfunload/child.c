// Calls the ModuleMgr functions that act on the caller's own module, from this module's code.

#include <pspkernel.h>

#include "shared.h"

PSP_MODULE_INFO("selfunloadchild", 0, 1, 1);

int sceKernelStopUnloadSelfModuleWithStatus(int exitCode, SceSize argSize, void *argp, int *status, SceKernelSMOption *option);
int sceKernelGetModuleId(void);
int sceKernelGetModuleGPByAddress(u32 addr, u32 *gp);

static Control *control;
static int stopArg = 0x5678;

static int worker(SceSize args, void *argp) {
	int result = 0;
	switch (control->call) {
	case CALL_SELF_STOP_UNLOAD:
		result = sceKernelSelfStopUnloadModule(control->exitCode, sizeof(stopArg), &stopArg);
		break;
	case CALL_STOP_UNLOAD_SELF:
		result = sceKernelStopUnloadSelfModule(sizeof(stopArg), &stopArg, &control->status, NULL);
		break;
	case CALL_STOP_UNLOAD_SELF_WITH_STATUS:
		result = sceKernelStopUnloadSelfModuleWithStatus(control->exitCode, sizeof(stopArg), &stopArg, &control->status, NULL);
		break;
	case CALL_STOP_UNLOAD_SELF_BAD_STATUS:
		result = sceKernelStopUnloadSelfModule(sizeof(stopArg), &stopArg, (int *)0x88000000, NULL);
		break;
	}
	control->callReturned = 1;
	control->callResult = result;
	return 0x777;
}

static int getModuleId(void) {
	return sceKernelGetModuleId();
}

static int getModuleIdByAddress(u32 addr) {
	return sceKernelGetModuleIdByAddress((const void *)addr);
}

static int getModuleGPByAddress(u32 addr, u32 *gp) {
	return sceKernelGetModuleGPByAddress(addr, gp);
}

int module_start(SceSize args, void *argp) {
	if (args != sizeof(Control *)) {
		return 0;
	}
	control = *(Control **)argp;
	control->worker = &worker;
	control->getModuleId = &getModuleId;
	control->getModuleIdByAddress = &getModuleIdByAddress;
	control->getModuleGPByAddress = &getModuleGPByAddress;
	control->childData = (u32)&stopArg;
	return 0;
}

int module_stop(SceSize args, void *argp) {
	control->stopCalls++;
	control->stopArgSize = args;
	control->stopArg = args >= 4 ? *(int *)argp : -1;
	return control->stopReturn;
}
