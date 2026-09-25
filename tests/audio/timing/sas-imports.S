	.set noreorder

#include "pspimport.s"

// The sceSasCore calls sastiming uses, from ../sascore/sascore-imports.S.
IMPORT_START "sceSasCore",0x00090011
IMPORT_FUNC  "sceSasCore",0x42778A9F,__sceSasInit
IMPORT_FUNC  "sceSasCore",0xA3589D81,__sceSasCore
IMPORT_FUNC  "sceSasCore",0x440CA7D8,__sceSasSetVolume
IMPORT_FUNC  "sceSasCore",0xAD84D37F,__sceSasSetPitch
IMPORT_FUNC  "sceSasCore",0x76F01ACA,__sceSasSetKeyOn
IMPORT_FUNC  "sceSasCore",0xA0CF2FA4,__sceSasSetKeyOff
IMPORT_FUNC  "sceSasCore",0xF983B186,__sceSasRevVON
IMPORT_FUNC  "sceSasCore",0xD5A229C9,__sceSasRevEVOL
IMPORT_FUNC  "sceSasCore",0x33D4AB37,__sceSasRevType
IMPORT_FUNC  "sceSasCore",0xE1CD9561,__sceSasSetVoicePCM
IMPORT_FUNC  "sceSasCore",0x99944089,__sceSasSetVoice
IMPORT_FUNC  "sceSasCore",0xB7660A23,__sceSasSetNoise
IMPORT_FUNC  "sceSasCore",0x019B25EB,__sceSasSetADSR
IMPORT_FUNC  "sceSasCore",0xE855BF76,__sceSasSetOutputmode
IMPORT_FUNC  "sceSasCore",0x50A14DFC,__sceSasCoreWithMix
