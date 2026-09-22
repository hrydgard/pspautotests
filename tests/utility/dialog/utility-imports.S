.set noreorder

#include "pspimport.s"

// All the sceUtility imports, so none come from libpsputility: mixing its stubs with extra ones
// here leaves the import table out of order, and the calls land on the wrong functions.
IMPORT_START "sceUtility",0x40010000

IMPORT_FUNC "sceUtility",0x946963F3,sceUtilityGameSharingGetStatus
IMPORT_FUNC "sceUtility",0xC492F751,sceUtilityGameSharingInitStart
IMPORT_FUNC "sceUtility",0xEFC6F80F,sceUtilityGameSharingShutdownStart
IMPORT_FUNC "sceUtility",0x7853182D,sceUtilityGameSharingUpdate
IMPORT_FUNC "sceUtility",0xA5DA2406,sceUtilityGetSystemParamInt
IMPORT_FUNC "sceUtility",0xBDA7D894,sceUtilityHtmlViewerGetStatus
IMPORT_FUNC "sceUtility",0x4928BD96,sceUtilityMsgDialogAbort
IMPORT_FUNC "sceUtility",0x9A1C91D7,sceUtilityMsgDialogGetStatus
IMPORT_FUNC "sceUtility",0x2AD8E239,sceUtilityMsgDialogInitStart
IMPORT_FUNC "sceUtility",0x67AF3428,sceUtilityMsgDialogShutdownStart
IMPORT_FUNC "sceUtility",0x95FC253B,sceUtilityMsgDialogUpdate
IMPORT_FUNC "sceUtility",0x6332AA39,sceUtilityNetconfGetStatus
IMPORT_FUNC "sceUtility",0xF3F76017,sceUtilityOskGetStatus
IMPORT_FUNC "sceUtility",0x8874DBE0,sceUtilitySavedataGetStatus
IMPORT_FUNC "sceUtility",0x50C4CD57,sceUtilitySavedataInitStart
IMPORT_FUNC "sceUtility",0x9790B33C,sceUtilitySavedataShutdownStart
IMPORT_FUNC "sceUtility",0xD4B95FFB,sceUtilitySavedataUpdate
IMPORT_FUNC "sceUtility",0xD81957B7,sceUtilityScreenshotGetStatus
IMPORT_FUNC "sceUtility",0xB57E95D9,sceUtilityGamedataInstallGetStatus
IMPORT_FUNC "sceUtility",0x86ABDB1B,sceUtilityNpSigninGetStatus
IMPORT_FUNC "sceUtility",0xCDC3AA41,sceUtilityHtmlViewerInitStart
IMPORT_FUNC "sceUtility",0xF5CE1134,sceUtilityHtmlViewerShutdownStart
IMPORT_FUNC "sceUtility",0x05AFB9E4,sceUtilityHtmlViewerUpdate
