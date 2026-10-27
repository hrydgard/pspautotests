.set noreorder

#include "pspimport.s"

IMPORT_START "sceNetApctl",0x00090000
IMPORT_FUNC	"sceNetApctl",0xE2F91F9B,sceNetApctlInit
IMPORT_FUNC	"sceNetApctl",0xB3EDD0EC,sceNetApctlTerm
IMPORT_FUNC	"sceNetApctl",0xCFB957C6,sceNetApctlConnect
IMPORT_FUNC	"sceNetApctl",0x24FE91A1,sceNetApctlDisconnect
IMPORT_FUNC	"sceNetApctl",0x5DEAC81B,sceNetApctlGetState
IMPORT_FUNC	"sceNetApctl",0x2BEFDF23,sceNetApctlGetInfo
IMPORT_FUNC	"sceNetApctl",0x8ABADD51,sceNetApctlAddHandler

IMPORT_START "sceNetResolver",0x00090000
IMPORT_FUNC	"sceNetResolver",0xF3370E61,sceNetResolverInit
IMPORT_FUNC	"sceNetResolver",0x6138194A,sceNetResolverTerm
IMPORT_FUNC	"sceNetResolver",0x244172AF,sceNetResolverCreate
IMPORT_FUNC	"sceNetResolver",0x94523E09,sceNetResolverDelete
IMPORT_FUNC	"sceNetResolver",0x224C5F44,sceNetResolverStartNtoA
IMPORT_FUNC	"sceNetResolver",0x629E2FB7,sceNetResolverStartAtoN
IMPORT_FUNC	"sceNetResolver",0x808F6063,sceNetResolverStop
