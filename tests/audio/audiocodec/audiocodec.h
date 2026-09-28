#pragma once

#include <common.h>

#define AUDIOCODEC_AT3PLUS 0x00001000
#define AUDIOCODEC_AT3 0x00001001
#define AUDIOCODEC_MP3 0x00001002
#define AUDIOCODEC_AAC 0x00001003

#ifdef __cplusplus
extern "C" {
#endif

// The context the Media Engine works on, laid out as PPSSPP's Core/HLE/sceAudiocodec.h documents
// it. 0x00..0x27 are common to all codecs; 0x28.. is per codec.
typedef struct {
	u32 magic;            // 0x00
	s32 unk4;             // 0x04
	s32 err;              // 0x08
	u32 edramAddr;        // 0x0c  in ME memory
	s32 neededMem;        // 0x10
	s32 inited;           // 0x14
	void *inBuf;          // 0x18
	s32 srcBytesRead;     // 0x1c
	void *outBuf;         // 0x20
	s32 dstBytesWritten;  // 0x24
	union {
		struct {
			u8 formatByte1;  // 0x28
			u8 formatByte2;  // 0x29  frame size = formatByte2 * 8 + 8
			u8 unk2a;
			u8 unk2b;
			u32 unk2c;
			u32 at3Related;  // 0x30
		} at3;
		struct {
			u32 maxFrameBytes;    // 0x28
			u32 unk2c;
			s32 unk30;
			s32 unk34;
			s32 version;          // 0x38
			s32 unk3c;
			s32 unk40;
			s32 bitrateIndex;     // 0x44
			s32 sampleRateIndex;  // 0x48
			s32 unk4c;
			s32 unk50;
			s32 channelConfig;    // 0x54
		} mp3;
		u8 raw[0x40];
	} fmt;
	u32 unk68;            // 0x68
	u8 unk[0x14];         // 0x6c
} SceAudiocodecCodec;

int sceAudiocodecGetInfo(SceAudiocodecCodec *ctx, int codec);
int sceAudiocodecCheckNeedMem(SceAudiocodecCodec *ctx, int codec);
int sceAudiocodecGetEDRAM(SceAudiocodecCodec *ctx, int codec);
int sceAudiocodecReleaseEDRAM(SceAudiocodecCodec *ctx);
int sceAudiocodecDecode(SceAudiocodecCodec *ctx, int codec);
int sceAudiocodecInitMono(SceAudiocodecCodec *ctx, int codec);
int sceAudiocodecInit(SceAudiocodecCodec *ctx, int codec);

#ifdef __cplusplus
}
#endif
