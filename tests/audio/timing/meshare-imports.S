	.set noreorder

#include "pspimport.s"

// The mpeg.prx calls meshare uses (as video/mp4/mp4-imports.S).
IMPORT_START "sceMpeg",0x00090000
IMPORT_FUNC  "sceMpeg",0x682A619B,sceMpegInit
IMPORT_FUNC  "sceMpeg",0x874624D6,sceMpegFinish
IMPORT_FUNC  "sceMpeg",0xC132E22F,sceMpegQueryMemSize
IMPORT_FUNC  "sceMpeg",0xD8C5F121,sceMpegCreate
IMPORT_FUNC  "sceMpeg",0x606A4649,sceMpegDelete
IMPORT_FUNC  "sceMpeg",0xFCBDB5AD,sceMpegAvcResourceInit
IMPORT_FUNC  "sceMpeg",0x8160A2FE,sceMpegAvcResourceFinish
IMPORT_FUNC  "sceMpeg",0x63B9536A,sceMpegAvcResourceGetAvcDecTopAddr
