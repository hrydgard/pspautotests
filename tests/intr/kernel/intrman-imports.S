.set noreorder

#include "pspimport.s"

IMPORT_START "InterruptManagerForKernel",0x00010000
IMPORT_FUNC	"InterruptManagerForKernel",0x58DD8978,sceKernelRegisterIntrHandler
IMPORT_FUNC	"InterruptManagerForKernel",0xF987B1F0,sceKernelReleaseIntrHandler
IMPORT_FUNC	"InterruptManagerForKernel",0x4D6E7305,sceKernelEnableIntr
IMPORT_FUNC	"InterruptManagerForKernel",0xD774BA45,sceKernelDisableIntr
