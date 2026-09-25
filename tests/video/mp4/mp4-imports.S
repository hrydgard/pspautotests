	.set noreorder

#include "pspimport.s"

// pspsdk has no sceMp4 library, and lacks the sceMpegAvcResource functions sceMp4 playback needs.
// NIDs as imported by a game that plays MP4 movies (Meruru no Atelier Plus Official PlayView).

IMPORT_START "sceMp4",0x00090000
IMPORT_FUNC  "sceMp4",0x68651CBC,sceMp4Init
IMPORT_FUNC  "sceMp4",0x9042B257,sceMp4Finish
IMPORT_FUNC  "sceMp4",0xB1221EE7,sceMp4Create
IMPORT_FUNC  "sceMp4",0x538C2057,sceMp4Delete
IMPORT_FUNC  "sceMp4",0x7443AF1D,sceMp4GetMovieInfo
IMPORT_FUNC  "sceMp4",0x5EB65F26,sceMp4GetNumberOfSpecificTrack
IMPORT_FUNC  "sceMp4",0x7ADFD01C,sceMp4RegistTrack
IMPORT_FUNC  "sceMp4",0x6710FE77,sceMp4UnregistTrack
IMPORT_FUNC  "sceMp4",0xBCA9389C,sceMp4TrackSampleBufQueryMemSize
IMPORT_FUNC  "sceMp4",0x9C8F4FC1,sceMp4TrackSampleBufConstruct
IMPORT_FUNC  "sceMp4",0x01C76489,sceMp4TrackSampleBufDestruct
IMPORT_FUNC  "sceMp4",0x0F0187D2,sceMp4GetAvcTrackInfoData
IMPORT_FUNC  "sceMp4",0x9CE6F5CF,sceMp4GetAacTrackInfoData
IMPORT_FUNC  "sceMp4",0x496E8A65,sceMp4TrackSampleBufFlush
IMPORT_FUNC  "sceMp4",0xD8250B75,sceMp4PutSampleNum
IMPORT_FUNC  "sceMp4",0x8754ECB8,sceMp4TrackSampleBufAvailableSize
IMPORT_FUNC  "sceMp4",0x31BCD7E0,sceMp4TrackSampleBufPut
IMPORT_FUNC  "sceMp4",0x503A3CBA,sceMp4GetAvcAu
IMPORT_FUNC  "sceMp4",0x5601A6F0,sceMp4GetAacAu
IMPORT_FUNC  "sceMp4",0x4ED4AB1E,sceMp4AacDecodeInitResource
IMPORT_FUNC  "sceMp4",0x10EE0D2C,sceMp4AacDecodeInit
IMPORT_FUNC  "sceMp4",0x7663CB5C,sceMp4AacDecode
IMPORT_FUNC  "sceMp4",0x5D72B333,sceMp4AacDecodeExit
IMPORT_FUNC  "sceMp4",0x7D332394,sceMp4AacDecodeTermResource

IMPORT_START "sceMpeg",0x00090000
IMPORT_FUNC  "sceMpeg",0x682A619B,sceMpegInit
IMPORT_FUNC  "sceMpeg",0x874624D6,sceMpegFinish
IMPORT_FUNC  "sceMpeg",0xC132E22F,sceMpegQueryMemSize
IMPORT_FUNC  "sceMpeg",0xD8C5F121,sceMpegCreate
IMPORT_FUNC  "sceMpeg",0x606A4649,sceMpegDelete
IMPORT_FUNC  "sceMpeg",0x167AFD9E,sceMpegInitAu
IMPORT_FUNC  "sceMpeg",0x0E3C2E9D,sceMpegAvcDecode
IMPORT_FUNC  "sceMpeg",0x740FCCD1,sceMpegAvcDecodeStop
IMPORT_FUNC  "sceMpeg",0xFCBDB5AD,sceMpegAvcResourceInit
IMPORT_FUNC  "sceMpeg",0x8160A2FE,sceMpegAvcResourceFinish
IMPORT_FUNC  "sceMpeg",0x63B9536A,sceMpegAvcResourceGetAvcDecTopAddr
IMPORT_FUNC  "sceMpeg",0xAF26BB01,sceMpegAvcResourceGetAvcEsBuf
