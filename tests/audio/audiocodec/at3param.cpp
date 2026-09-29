#include <common.h>

#include <psputility.h>
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "audiocodec.h"

// Atrac3 frames through sceAudiocodec directly, by codec parameter, using the synthetic frames from
// ../atrac/gen_atrac3.py. atrac3_c0_mono.at3 has 0xC0-byte frames of one mono sound unit each
// behind a 2-channel header (which libatrac3plus turns into parameter 0x0E, as for LocoRoco 2's
// MuiMui house music). atrac3_180_stereo.raw is four normal-stereo 0x180 frames.

static SceAudiocodecCodec ctx __attribute__((aligned(64)));
static short pcm[1024 * 2] __attribute__((aligned(64)));

static u8 *loadFile(const char *path, int *size) {
	SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
	if (fd < 0) {
		checkpoint("Can't open %s: %08x", path, fd);
		return NULL;
	}
	*size = sceIoLseek32(fd, 0, PSP_SEEK_END);
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	u8 *data = (u8 *)memalign(64, *size);
	sceIoRead(fd, data, *size);
	sceIoClose(fd);
	sceKernelDcacheWritebackRange(data, *size);
	return data;
}

// Decodes every frame; reports what the first frame's call returned, how many decoded, and what
// the output looked like. Samples aren't compared: our decoder isn't bit exact with the ME's.
static void run(const char *title, const u8 *frames, int count, u32 param, int frameSize) {
	memset(&ctx, 0, sizeof(ctx));
	*(u32 *)ctx.fmt.raw = param;
	sceAudiocodecCheckNeedMem(&ctx, AUDIOCODEC_AT3);
	sceAudiocodecGetEDRAM(&ctx, AUDIOCODEC_AT3);
	int result = sceAudiocodecInit(&ctx, AUDIOCODEC_AT3);
	checkpoint("%s: init %08x", title, result);
	if (result < 0) {
		sceAudiocodecReleaseEDRAM(&ctx);
		return;
	}

	int decoded = 0;
	bool audible = false;
	bool leftIsRight = true;
	for (int i = 0; i < count; ++i) {
		ctx.inBuf = (void *)(frames + i * frameSize);
		ctx.outBuf = pcm;
		memset(pcm, 0, sizeof(pcm));
		sceKernelDcacheWritebackInvalidateRange(pcm, sizeof(pcm));
		result = sceAudiocodecDecode(&ctx, AUDIOCODEC_AT3);
		sceKernelDcacheInvalidateRange(pcm, sizeof(pcm));
		if (i == 0) {
			// ctx.err also differs by cause: 0x183 for the mono frames under 0x0B, 0x182 for the
			// half-zeroed stereo ones. We report 0x182 for both, so it isn't printed.
			checkpoint("  frame 0: %08x, read %d, wrote %d", result, (int)ctx.srcBytesRead, (int)ctx.dstBytesWritten);
		}
		if (result < 0) {
			continue;
		}
		decoded++;
		for (int j = 0; j < ctx.dstBytesWritten / 4; ++j) {
			if (pcm[j * 2] != 0 || pcm[j * 2 + 1] != 0) {
				audible = true;
			}
			if (pcm[j * 2] != pcm[j * 2 + 1]) {
				leftIsRight = false;
			}
		}
	}
	checkpoint("  %d of %d frames decoded, audible=%d, left==right=%d", decoded, count, audible, leftIsRight);
	sceAudiocodecReleaseEDRAM(&ctx);
}

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);

	int size = 0;
	u8 *file = loadFile("atrac3_c0_mono.at3", &size);
	if (!file) {
		return 1;
	}
	const u8 *frames = file + 0x4C;
	const int count = (size - 0x4C) / 0xC0;

	checkpointNext("0xC0 frames by parameter:");
	run("  0x0E (0xC0 mono)", frames, count, 0x0E, 0xC0);
	run("  0x0B (0xC0 joint stereo)", frames, count, 0x0B, 0xC0);

	// Normal-stereo frames, whole and with the second channel's half zeroed.
	int stereoSize = 0;
	u8 *stereo = loadFile("atrac3_180_stereo.raw", &stereoSize);
	if (stereo) {
		checkpointNext("0x180 stereo frames, parameter 0x04:");
		run("  Whole", stereo, stereoSize / 0x180, 0x04, 0x180);
		for (int i = 0; i < stereoSize / 0x180; ++i) {
			memset(stereo + i * 0x180 + 0xC0, 0, 0xC0);
		}
		sceKernelDcacheWritebackRange(stereo, stereoSize);
		run("  Second channel all zero", stereo, stereoSize / 0x180, 0x04, 0x180);
		free(stereo);
	}

	free(file);
	sceUtilityUnloadModule(PSP_MODULE_AV_AVCODEC);
	return 0;
}
