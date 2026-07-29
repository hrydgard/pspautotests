.set noreorder

#include "pspimport.s"

IMPORT_START "sceReg",0x40010000

IMPORT_FUNC "sceReg",0x92E41280,sceRegOpenRegistry
IMPORT_FUNC "sceReg",0xFA8A5739,sceRegCloseRegistry
IMPORT_FUNC "sceReg",0xDEDA92BF,sceRegRemoveRegistry
IMPORT_FUNC "sceReg",0x1D8A762E,sceRegOpenCategory
IMPORT_FUNC "sceReg",0x0CAE832B,sceRegCloseCategory
IMPORT_FUNC "sceReg",0x39461B4D,sceRegFlushRegistry
IMPORT_FUNC "sceReg",0x0D69BF40,sceRegFlushCategory
IMPORT_FUNC "sceReg",0x57641A81,sceRegCreateKey
IMPORT_FUNC "sceReg",0x17768E14,sceRegSetKeyValue
IMPORT_FUNC "sceReg",0xD4475AA8,sceRegGetKeyInfo
IMPORT_FUNC "sceReg",0x28A8E98A,sceRegGetKeyValue
IMPORT_FUNC "sceReg",0x2C0DB9DD,sceRegGetKeysNum
IMPORT_FUNC "sceReg",0x2D211135,sceRegGetKeys
IMPORT_FUNC "sceReg",0xC5768D02,sceRegGetKeyInfoByName
IMPORT_FUNC "sceReg",0x30BE0259,sceRegGetKeyValueByName
IMPORT_FUNC "sceReg",0x4CA16893,sceRegRemoveCategory
IMPORT_FUNC "sceReg",0x3615BC87,sceRegRemoveKey
IMPORT_FUNC "sceReg",0x9B25EDF1,sceRegExit
IMPORT_FUNC "sceReg",0xBE8C1263,sceRegGetCategoryNumAtRoot
IMPORT_FUNC "sceReg",0x835ECE6F,sceRegGetCategoryListAtRoot