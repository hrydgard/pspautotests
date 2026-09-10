.set noreorder

#include "pspimport.s"

IMPORT_START "sceParseUri",0x40090000
IMPORT_FUNC  "sceParseUri",0x568518C9,sceUriParse
IMPORT_FUNC  "sceParseUri",0x7EE318AF,sceUriBuild
IMPORT_FUNC  "sceParseUri",0x49E950EC,sceUriEscape
IMPORT_FUNC  "sceParseUri",0x062BB07E,sceUriUnescape
