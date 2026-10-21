.set noreorder

#include "pspimport.s"

# A kernel module can't import ModuleMgrForUser. 6.60/6.61 NIDs, matched to the ModuleMgrForUser
# functions at the same addresses in modulemgr.prx (6.61).
IMPORT_START "ModuleMgrForKernel",0x00090000
IMPORT_FUNC	"ModuleMgrForKernel",0x939E4270,kernelLoadModule
IMPORT_FUNC	"ModuleMgrForKernel",0x387E3CA9,kernelUnloadModule
