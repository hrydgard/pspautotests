#include <common.h>

#define AUDIOCODEC_AT3PLUS 0x00001000
#define AUDIOCODEC_AT3 0x00001001
#define AUDIOCODEC_MP3 0x00001002
#define AUDIOCODEC_AAC 0x00001003

const char *AudioCodecToString(int codecType);

#ifdef __cplusplus
extern "C" {
#endif
	// From PPSSPP.
	typedef struct {
		s32 unk_init;
		s32 unk4;
		s32 err; // 8
		s32 edramAddr; // 12  // 18eac0 ?? I guess this is in ME space?
		s32 neededMem; // 16  // 0x102400
		s32 inited;
		void *inBuf; // 24  // This is updated for every frame that's decoded, to point to the start of the frame.
		s32 inBytes;
		void *outBuf; // 32
		s32 outBytes;
		s8 tailRelated;
		s8 tailFlag;
		s8 unk42;
		s8 unk43;
		s8 unk44;
		s8 unk45;
		s8 unk46;
		s8 unk47;
		s32 unk48;
		s32 unk52;
		s32 unk56;
		s32 unk60;
		s32 unk64;
		s32 unk68;
		s32 unk72;
		s32 unk76;
		s32 unk80;
		s32 unk84;
		s32 unk88;
		s32 unk92;
		s32 unk96;
		s32 unk100;
		void *allocMem; // 104
		// make sure the size is 128
		u8 unk[20];
	} SceAudiocodecCodec;

    int sceAudiocodecGetInfo(SceAudiocodecCodec *ctxPtr, int codec);
    int sceAudiocodecCheckNeedMem(SceAudiocodecCodec * ctxPtr, int codec);
    int sceAudiocodecGetEDRAM(SceAudiocodecCodec *ctxPtr, int codec);
    int sceAudiocodecReleaseEDRAM(SceAudiocodecCodec *ctxPtr, int id);
    int sceAudiocodecDecode(SceAudiocodecCodec *ctxPtr, int codec);
    int sceAudiocodecInitMono(SceAudiocodecCodec *ctxPtr, int codec);
    int sceAudiocodecInit(SceAudiocodecCodec *ctxPtr, int codec);

#ifdef __cplusplus
}
#endif
