.set noreorder

#include "pspimport.s"

IMPORT_START "ModuleMgrForUser",0x40010000
IMPORT_FUNC	"ModuleMgrForUser",0xF0A26395,sceKernelGetModuleId
IMPORT_FUNC	"ModuleMgrForUser",0x50F0C1EC,sceKernelStartModule
IMPORT_FUNC	"ModuleMgrForUser",0xD1FF982A,sceKernelStopModule
