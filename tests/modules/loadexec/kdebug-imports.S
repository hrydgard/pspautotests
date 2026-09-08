.set noreorder

#include "pspimport.s"

IMPORT_START "KDebugForKernel",0x40010000
IMPORT_FUNC	"KDebugForKernel",0x84F370BC,Kprintf
