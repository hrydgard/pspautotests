#include <common.h>

#include <psputility.h>
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "audiocodec.h"

// Atrac3 through sceAudiocodec: the error each kind of bad frame gives, and what InitMono does. Uses
// the synthetic frames from ../atrac/gen_atrac3.py.
//
// ctx.err is 0x183 for a joint stereo frame whose second part lacks its marker (a mono unit decoded
// as joint stereo), and 0x182 for any other bad sound unit, on either channel. InitMono only takes
// the mono parameters, and writes Init's left channel as plain mono. What decides that is the word
// at 0x34, which Init sets to 2 and InitMono to 1: a mono layout is decoded to stereo only when it's
// 2, whichever init set it up.

static SceAudiocodecCodec ctx __attribute__((aligned(64)));
static short pcm[1024 * 2] __attribute__((aligned(64)));
static short monoOut[1024 * 2] __attribute__((aligned(64)));
static u8 frame[0x180] __attribute__((aligned(64)));

static u8 *loadFile(const char *path, int *size) {
	SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
	if (fd < 0) {
		return NULL;
	}
	*size = sceIoLseek32(fd, 0, PSP_SEEK_END);
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	u8 *data = (u8 *)memalign(64, *size);
	sceIoRead(fd, data, *size);
	sceIoClose(fd);
	return data;
}

static int decodeWith(u32 param, bool mono, const u8 *src, int len, short *out) {
	memset(&ctx, 0, sizeof(ctx));
	*(u32 *)ctx.fmt.raw = param;
	sceAudiocodecCheckNeedMem(&ctx, AUDIOCODEC_AT3);
	sceAudiocodecGetEDRAM(&ctx, AUDIOCODEC_AT3);
	int r = mono ? sceAudiocodecInitMono(&ctx, AUDIOCODEC_AT3) : sceAudiocodecInit(&ctx, AUDIOCODEC_AT3);
	if (mono) {
		checkpoint("  InitMono(%02x): %08x", (int)param, r);
	}
	if (r < 0) {
		sceAudiocodecReleaseEDRAM(&ctx);
		return r;
	}
	memcpy(frame, src, len);
	sceKernelDcacheWritebackRange(frame, sizeof(frame));
	ctx.inBuf = frame;
	ctx.outBuf = out;
	memset(out, 0, 4096);
	sceKernelDcacheWritebackInvalidateRange(out, 4096);
	r = sceAudiocodecDecode(&ctx, AUDIOCODEC_AT3);
	sceKernelDcacheInvalidateRange(out, 4096);
	sceAudiocodecReleaseEDRAM(&ctx);
	return r;
}

static void tryErr(const char *title, u32 param, const u8 *src, int len) {
	int r = decodeWith(param, false, src, len, pcm);
	checkpoint("  %s: %08x err %03x read %d wrote %d", title, r, (int)ctx.err, (int)ctx.srcBytesRead, (int)ctx.dstBytesWritten);
}

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	int monoSize = 0, stereoSize = 0;
	u8 *monoFile = loadFile("atrac3_c0_mono.at3", &monoSize);
	u8 *stereo = loadFile("atrac3_180_stereo.raw", &stereoSize);
	const u8 *mono = monoFile + 0x4C + 0xC0 * 10;

	checkpointNext("Bad frames:");
	u8 bad[0x180];
	// Mono 0x0E.
	tryErr("0E good", 0x0E, mono, 0xC0);
	memcpy(bad, mono, 0xC0); bad[0] = 0x00; tryErr("0E id wrong", 0x0E, bad, 0xC0);
	memcpy(bad, mono, 0xC0); memset(bad, 0, 0xC0); tryErr("0E all zero", 0x0E, bad, 0xC0);
	memcpy(bad, mono, 0xC0); memset(bad, 0xFF, 0xC0); tryErr("0E all FF", 0x0E, bad, 0xC0);
	// Stereo 0x04.
	tryErr("04 good", 0x04, stereo, 0x180);
	memcpy(bad, stereo, 0x180); bad[0] = 0; tryErr("04 ch0 id wrong", 0x04, bad, 0x180);
	memcpy(bad, stereo, 0x180); bad[0xC0] = 0; tryErr("04 ch1 id wrong", 0x04, bad, 0x180);
	memcpy(bad, stereo, 0x180); memset(bad + 0xC0, 0, 0xC0); tryErr("04 ch1 zero", 0x04, bad, 0x180);
	memcpy(bad, stereo, 0x180); memset(bad, 0xFF, 0x180); tryErr("04 all FF", 0x04, bad, 0x180);
	// Joint 0x0B with a mono unit (valid first unit, the second part isn't a joint unit).
	tryErr("0B mono data", 0x0B, mono, 0xC0);
	memcpy(bad, mono, 0xC0); bad[0] = 0; tryErr("0B id wrong", 0x0B, bad, 0xC0);
	memcpy(bad, mono, 0xC0); memset(bad, 0, 0xC0); tryErr("0B all zero", 0x0B, bad, 0xC0);

	checkpointNext("InitMono:");
	// Mono stream under InitMono vs Init.
	decodeWith(0x0E, false, mono, 0xC0, pcm);
	int r = decodeWith(0x0E, true, mono, 0xC0, monoOut);
	int n = ctx.dstBytesWritten / 2;
	int eqL = 0;
	for (int i = 0; i < n && i < 1024; ++i) {
		eqL += monoOut[i] == pcm[i * 2];
	}
	checkpoint("  0E: %08x wrote %d; equal to Init's left %d of %d", r, (int)ctx.dstBytesWritten, eqL, n);

	// A stereo parameter: InitMono fails.
	r = decodeWith(0x04, true, stereo, 0x180, monoOut);
	checkpoint("  04: %08x wrote %d", r, (int)ctx.dstBytesWritten);
	checkpointNext("Context:");
	static const u32 monoParams[] = { 0x0E, 0x0F };
	for (u32 param : monoParams) {
		for (int m = 0; m < 2; ++m) {
			memset(&ctx, 0, sizeof(ctx));
			*(u32 *)ctx.fmt.raw = param;
			sceAudiocodecCheckNeedMem(&ctx, AUDIOCODEC_AT3);
			sceAudiocodecGetEDRAM(&ctx, AUDIOCODEC_AT3);
			int ri = m ? sceAudiocodecInitMono(&ctx, AUDIOCODEC_AT3) : sceAudiocodecInit(&ctx, AUDIOCODEC_AT3);
			const u32 *f = (const u32 *)ctx.fmt.raw;
			checkpoint("  %s(%02x): %08x, 0x2c %x, 0x30 %x, 0x34 %x", m ? "InitMono" : "Init", (int)param, ri, (unsigned)f[1], (unsigned)f[2], (unsigned)f[3]);
			sceAudiocodecReleaseEDRAM(&ctx);
		}
	}
	static const struct { u32 param; bool mono; int value; } pokes[] = {
		{ 0x0E, false, 1 }, { 0x0E, false, 0 }, { 0x0E, false, 3 }, { 0x0E, true, 2 }, { 0x04, false, 1 },
	};
	for (const auto &poke : pokes) {
		memset(&ctx, 0, sizeof(ctx));
		*(u32 *)ctx.fmt.raw = poke.param;
		sceAudiocodecCheckNeedMem(&ctx, AUDIOCODEC_AT3);
		sceAudiocodecGetEDRAM(&ctx, AUDIOCODEC_AT3);
		if (poke.mono) {
			sceAudiocodecInitMono(&ctx, AUDIOCODEC_AT3);
		} else {
			sceAudiocodecInit(&ctx, AUDIOCODEC_AT3);
		}
		((u32 *)ctx.fmt.raw)[3] = poke.value;
		const bool stereoFrame = poke.param == 0x04;
		memcpy(frame, stereoFrame ? stereo : mono, stereoFrame ? 0x180 : 0xC0);
		sceKernelDcacheWritebackRange(frame, sizeof(frame));
		ctx.inBuf = frame;
		ctx.outBuf = pcm;
		int rd = sceAudiocodecDecode(&ctx, AUDIOCODEC_AT3);
		checkpoint("  %s(%02x), then 0x34 = %d: decode %08x wrote %d", poke.mono ? "InitMono" : "Init", (int)poke.param, poke.value, rd, (int)ctx.dstBytesWritten);
		sceAudiocodecReleaseEDRAM(&ctx);
	}

	checkpointNext("InitMono by parameter:");
	// All of them, just the init.
	static const u32 params[] = { 0x04, 0x06, 0x0B, 0x0E, 0x0F };
	for (u32 param : params) {
		memset(&ctx, 0, sizeof(ctx));
		*(u32 *)ctx.fmt.raw = param;
		sceAudiocodecCheckNeedMem(&ctx, AUDIOCODEC_AT3);
		sceAudiocodecGetEDRAM(&ctx, AUDIOCODEC_AT3);
		int ri = sceAudiocodecInitMono(&ctx, AUDIOCODEC_AT3);
		checkpoint("  Param %02x: %08x", (int)param, ri);
		sceAudiocodecReleaseEDRAM(&ctx);
	}
	return 0;
}
