# 6.60/6.61 NIDs the POPS manager calls to set up audio (popsman.prx +342C).
	.set noreorder

#include "pspimport.s"

	IMPORT_START "sceSysreg_driver",0x00010000
	IMPORT_FUNC  "sceSysreg_driver",0xC36775AD,SysregC36775AD
	IMPORT_FUNC  "sceSysreg_driver",0xB1751B06,SysregB1751B06
	IMPORT_FUNC  "sceSysreg_driver",0x4634C9DC,Sysreg4634C9DC
	IMPORT_FUNC  "sceSysreg_driver",0x41B0337B,Sysreg41B0337B
	IMPORT_FUNC  "sceSysreg_driver",0x4B4CCE80,Sysreg4B4CCE80

	IMPORT_START "sceSyscon_driver",0x00010000
	IMPORT_FUNC  "sceSyscon_driver",0x9BC5E33B,Syscon9BC5E33B

	IMPORT_START "sceClockgen_driver",0x00010000
	IMPORT_FUNC  "sceClockgen_driver",0xDAB6E612,ClockgenDAB6E612

	IMPORT_START "InterruptManagerForKernel",0x00010000
	IMPORT_FUNC  "InterruptManagerForKernel",0xD774BA45,IntrDisableD774BA45
