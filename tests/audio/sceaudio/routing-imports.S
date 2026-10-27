	.set noreorder

#include "pspimport.s"
IMPORT_START "sceAudioRouting",0x40010000
IMPORT_FUNC  "sceAudioRouting",0x39240E7D,sceAudioRoutingGetMode
IMPORT_FUNC  "sceAudioRouting",0x28235C56,sceAudioRoutingGetVolumeMode
IMPORT_FUNC  "sceAudioRouting",0x36FD8AA9,sceAudioRoutingSetMode
IMPORT_FUNC  "sceAudioRouting",0xBB548475,sceAudioRoutingSetVolumeMode
