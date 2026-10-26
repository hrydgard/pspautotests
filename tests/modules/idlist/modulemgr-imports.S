.set noreorder

#include "pspimport.s"

# Both from here: the SDK's ModuleMgrForUser stubs, linked next to an import of the same library,
# have been seen to do nothing.
IMPORT_START "ModuleMgrForUser",0x40010000
IMPORT_FUNC	"ModuleMgrForUser",0xF0A26395,sceKernelGetModuleId
IMPORT_FUNC	"ModuleMgrForUser",0x644395E2,sceKernelGetModuleIdList
