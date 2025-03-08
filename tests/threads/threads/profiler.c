#include "common.h"
#include "shared.h"
#include "string.h"

int main(int argc, char *argv[]) {
	CHECKPOINT_ENABLE_TIME = 1;
	int i;

	checkpointNext("sceKernelReferThreadProfiler");
	for (i = 0; i < 8; i++) {
		PspDebugProfilerRegs *regs = sceKernelReferThreadProfiler();
		checkpoint("regs: %08x", regs);
	}

	checkpointNext("sceKernelReferGlobalProfiler");
	for (i = 0; i < 8; i++) {
		PspDebugProfilerRegs *regs = sceKernelReferGlobalProfiler();
		checkpoint("regs: %08x", regs);
	}
	return 0;
}
