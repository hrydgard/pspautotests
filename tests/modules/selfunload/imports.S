	.set noreorder

#include "pspimport.s"

	IMPORT_START "ModuleMgrForUser",0x40010000
	IMPORT_FUNC "ModuleMgrForUser",0xD675EBB8,sceKernelSelfStopUnloadModule
	IMPORT_FUNC "ModuleMgrForUser",0xCC1D3699,sceKernelStopUnloadSelfModule
	IMPORT_FUNC "ModuleMgrForUser",0x8F2DF740,sceKernelStopUnloadSelfModuleWithStatus
	IMPORT_FUNC "ModuleMgrForUser",0xF0A26395,sceKernelGetModuleId
	IMPORT_FUNC "ModuleMgrForUser",0xD8B73127,sceKernelGetModuleIdByAddress
	IMPORT_FUNC "ModuleMgrForUser",0xD2FBC957,sceKernelGetModuleGPByAddress
