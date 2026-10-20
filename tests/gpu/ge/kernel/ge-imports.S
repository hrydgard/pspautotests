.set noreorder

#include "pspimport.s"

IMPORT_START "sceGe_driver",0x00010000
IMPORT_FUNC	"sceGe_driver",0x1F6752AD,sceGeEdramGetSize
IMPORT_FUNC	"sceGe_driver",0x547EC5F0,sceGeEdramGetHwSize
IMPORT_FUNC	"sceGe_driver",0xD8633888,sceGeEdramSetSize
