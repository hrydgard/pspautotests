.set noreorder

#include "pspimport.s"

IMPORT_START "InterruptManagerForKernel",0x00010000
IMPORT_FUNC	"InterruptManagerForKernel",0x092968F4,sceKernelCpuSuspendIntr
IMPORT_FUNC	"InterruptManagerForKernel",0x5F10D406,sceKernelCpuResumeIntr
