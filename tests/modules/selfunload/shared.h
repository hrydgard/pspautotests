// Shared between the test and its child module.
#include <pspkernel.h>

enum {
	CALL_SELF_STOP_UNLOAD,
	CALL_STOP_UNLOAD_SELF,
	CALL_STOP_UNLOAD_SELF_WITH_STATUS,
	CALL_STOP_UNLOAD_SELF_BAD_STATUS,
};

typedef struct {
	// Set by the child's module_start.
	SceKernelThreadEntry worker;
	int (*getModuleId)(void);
	int (*getModuleIdByAddress)(u32 addr);
	int (*getModuleGPByAddress)(u32 addr, u32 *gp);
	u32 childData;

	// Set by the test before starting the worker.
	int call;
	int exitCode;
	int stopReturn;

	// Filled in by the child.
	int stopCalls;
	int stopArgSize;
	int stopArg;
	int status;
	int callReturned;
	int callResult;
} Control;
