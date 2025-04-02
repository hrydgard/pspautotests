#include <common.h>

#include <psputility.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include <pspatrac3.h>
#include <pspaudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <psputility.h>


#include "shared.h"

static SceAudiocodecCodec g_codec;

void TestCodecBasics() {
	memset(&g_codec, 0, sizeof(g_codec));

	schedf("CheckNeedMem before init: %08x\n", sceAudiocodecCheckNeedMem(&g_codec, AUDIOCODEC_AT3PLUS));
	LogCodec(g_codec);

	// Not sure what this does, but it's important.
	g_codec.tailRelated = 0x28;
	g_codec.tailFlag = 0x5c;

	int retval = sceAudiocodecCheckNeedMem(&g_codec, AUDIOCODEC_AT3PLUS);
	schedf("sceAudiocodecCheckNeedMem at3+: %08x\n", retval);
	int neededMemAt3plus = (g_codec.neededMem + 0x3f) & ~0x3f;

	LogCodec(g_codec);

	retval = sceAudiocodecCheckNeedMem(&g_codec, AUDIOCODEC_AT3);
	schedf("sceAudiocodecCheckNeedMem at3: %08x\n", retval);
	int neededMemAt3 = (g_codec.neededMem + 0x3f) & ~0x3f;

	LogCodec(g_codec);

	g_codec.neededMem = 0x19000; // why?

	retval = sceAudiocodecGetEDRAM(&g_codec, AUDIOCODEC_AT3);
	schedf("sceAudiocodecGetEDRAM at3: %08x (addr: %08x)\n", retval, g_codec.edramAddr);

	g_codec.inited = 1;

	LogCodec(g_codec);

	int initRet = sceAudiocodecInit(&g_codec, AUDIOCODEC_AT3PLUS);
	schedf("Init: %08x\n", initRet);

	LogCodec(g_codec);
}

extern unsigned int CHECKPOINT_OUTPUT_DIRECT;
extern unsigned int HAS_DISPLAY;

extern "C" int main(int argc, char *argv[]) {
	CHECKPOINT_OUTPUT_DIRECT = 1;
	HAS_DISPLAY = 1;
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);

	TestCodecBasics();
	return 0;
}