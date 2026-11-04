.set noreorder

#include "pspimport.s"

IMPORT_START "sceNp",0x40090000
IMPORT_FUNC  "sceNp",0x857B47D3,sceNpInit
IMPORT_FUNC  "sceNp",0x37E1E274,sceNpTerm

IMPORT_START "sceNpAuth",0x40090000
IMPORT_FUNC  "sceNpAuth",0xA1DE86F8,sceNpAuthInit
IMPORT_FUNC  "sceNpAuth",0x4EC1F667,sceNpAuthTerm
IMPORT_FUNC  "sceNpAuth",0x5A3CB57A,sceNpAuthGetTicketParam
