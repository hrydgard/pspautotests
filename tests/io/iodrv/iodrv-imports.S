.set noreorder

#include "pspimport.s"

IMPORT_START "IoFileMgrForKernel",0x40010000
IMPORT_FUNC	"IoFileMgrForKernel",0x8E982A74,sceIoAddDrv
IMPORT_FUNC	"IoFileMgrForKernel",0xC7F35804,sceIoDelDrv
