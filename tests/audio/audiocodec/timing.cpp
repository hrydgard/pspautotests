// Times sceAudiocodecDecode for every codec at several bitrates, at the default and the highest
// clocks. The decode runs on the Media Engine and the call blocks until it's done, so this is
// what a game waits per frame. Timing probe, not a pass/fail test.
#include <common.h>

#include <psppower.h>
#include <psputility.h>
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "audiocodec.h"

// The test harness echoes stdout to the debug screen; it doesn't matter here, but it costs time.
extern unsigned int HAS_DISPLAY;

struct Stream {
	const char *name;
	const char *path;
	int codec;
};

static const Stream streams[] = {
	{ "at3+ 64k", "../atrac/sample.at3", AUDIOCODEC_AT3PLUS },
	{ "at3+ 128k", "../atrac/sample_long.at3", AUDIOCODEC_AT3PLUS },
	{ "at3 66k", "../atrac/test_mono.at3", AUDIOCODEC_AT3 },
	{ "mp3 32k 22khz", "mp3_22khz_32k.mp3", AUDIOCODEC_MP3 },
	{ "mp3 64k", "mp3_64k.mp3", AUDIOCODEC_MP3 },
	{ "mp3 128k", "mp3_128k.mp3", AUDIOCODEC_MP3 },
	{ "mp3 320k", "mp3_320k.mp3", AUDIOCODEC_MP3 },
	{ "aac 64k", "aac_64k.aac", AUDIOCODEC_AAC },
	{ "aac 128k", "aac_128k.aac", AUDIOCODEC_AAC },
	{ "aac 192k", "aac_192k.aac", AUDIOCODEC_AAC },
};

#define WARMUP 4
#define TIMED 32
// Enough for WARMUP + TIMED frames of any of the files; sample_long.at3 is much longer.
#define MAX_FILE 0x20000

static SceAudiocodecCodec ctx __attribute__((aligned(64)));
static short pcm[2048 * 2 * 2] __attribute__((aligned(64)));
static u8 frameBuf[0x800] __attribute__((aligned(64)));

static u8 *loadFile(const char *path, int *size) {
	SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
	if (fd < 0) {
		printf("Can't open %s: %08x\n", path, fd);
		return NULL;
	}
	*size = sceIoLseek32(fd, 0, PSP_SEEK_END);
	if (*size > MAX_FILE) {
		*size = MAX_FILE;
	}
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	u8 *data = (u8 *)memalign(64, *size);
	sceIoRead(fd, data, *size);
	sceIoClose(fd);
	return data;
}

static u32 read32(const u8 *p) {
	return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

// Finds the fmt and data chunks of a RIFF WAVE file.
static bool parseRiff(const u8 *file, int size, const u8 **fmt, int *dataOffset) {
	int pos = 12;
	*fmt = NULL;
	while (pos + 8 <= size) {
		u32 len = read32(file + pos + 4);
		if (memcmp(file + pos, "fmt ", 4) == 0) {
			*fmt = file + pos + 8;
		} else if (memcmp(file + pos, "data", 4) == 0) {
			*dataOffset = pos + 8;
			return *fmt != NULL;
		}
		pos += 8 + ((len + 1) & ~1);
	}
	return false;
}

// What libatrac3plus.prx's SetData (0880645c) stores at 0x28 for Atrac3, from its table keyed by
// frame size and joint stereo (the same five rows as PPSSPP's sceAudiocodec.cpp notes).
static int at3Param(int frameBytes, int jointStereo) {
	switch (frameBytes) {
	case 0x180: return 0x04;
	case 0x130: return 0x06;
	case 0xC0: return jointStereo ? 0x0B : 0x0E;
	case 0x98: return 0x0F;
	default: return -1;
	}
}

static int adtsSampleRate(const u8 *h) {
	static const int rates[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350 };
	int index = (h[2] >> 2) & 0xF;
	return index < 13 ? rates[index] : 0;
}

// Sets up the context the way the library that normally drives each codec does, and returns the
// offset of the first frame.
static int setup(const Stream &s, const u8 *file, int size, int *frameBytes) {
	memset(&ctx, 0, sizeof(ctx));
	*frameBytes = 0;
	int start = 0;
	if (s.codec == AUDIOCODEC_AT3PLUS || s.codec == AUDIOCODEC_AT3) {
		const u8 *fmt;
		if (!parseRiff(file, size, &fmt, &start)) {
			return -1;
		}
		*frameBytes = fmt[12] | (fmt[13] << 8);
		if (s.codec == AUDIOCODEC_AT3PLUS) {
			ctx.fmt.at3.formatByte1 = fmt[0x2a];
			ctx.fmt.at3.formatByte2 = fmt[0x2b];
			ctx.inited = 1;
		} else {
			int param = at3Param(*frameBytes, fmt[0x18] | (fmt[0x19] << 8));
			if (param < 0) {
				return -1;
			}
			*(u32 *)ctx.fmt.raw = param;
		}
	} else if (s.codec == AUDIOCODEC_AAC) {
		// libmp4.prx puts the sample rate here and passes raw access units.
		*(u32 *)ctx.fmt.raw = adtsSampleRate(file);
	}
	int result = sceAudiocodecCheckNeedMem(&ctx, s.codec);
	if (result >= 0) {
		result = sceAudiocodecGetEDRAM(&ctx, s.codec);
	}
	if (result >= 0) {
		result = sceAudiocodecInit(&ctx, s.codec);
		if (result < 0) {
			sceAudiocodecReleaseEDRAM(&ctx);
		}
	}
	if (result < 0) {
		printf("%-14s setup failed: %08x (err %08x)\n", s.name, result, (int)ctx.err);
		return -1;
	}
	if (s.codec == AUDIOCODEC_MP3) {
		// As libmp3.prx: the largest possible frame.
		ctx.fmt.mp3.maxFrameBytes = 0x5A1;
	}
	return start;
}

static void runStream(const Stream &s) {
	int size = 0;
	u8 *file = loadFile(s.path, &size);
	if (!file) {
		return;
	}
	int frameBytes;
	int pos = setup(s, file, size, &frameBytes);
	if (pos < 0) {
		free(file);
		return;
	}
	sceKernelDcacheWritebackRange(file, size);

	int minUs = 0x7FFFFFFF, maxUs = 0, sum = 0, timed = 0, failed = 0, frame;
	for (frame = 0; frame < WARMUP + TIMED; frame++) {
		int len = frameBytes;
		if (s.codec == AUDIOCODEC_AAC) {
			// Strip the ADTS header, which the MP4 container doesn't have.
			if (pos + 7 > size || file[pos] != 0xFF) {
				break;
			}
			int adtsLen = ((file[pos + 3] & 3) << 11) | (file[pos + 4] << 3) | (file[pos + 5] >> 5);
			int headerLen = (file[pos + 1] & 1) ? 7 : 9;
			len = adtsLen - headerLen;
			memcpy(frameBuf, file + pos + headerLen, len);
			sceKernelDcacheWritebackRange(frameBuf, sizeof(frameBuf));
			ctx.inBuf = frameBuf;
			pos += adtsLen;
		} else {
			if (pos + (len > 0 ? len : 4) > size) {
				break;
			}
			ctx.inBuf = file + pos;
		}
		ctx.outBuf = pcm;
		sceKernelDcacheWritebackInvalidateRange(pcm, sizeof(pcm));

		u32 t0 = sceKernelGetSystemTimeLow();
		int result = sceAudiocodecDecode(&ctx, s.codec);
		int us = (int)(sceKernelGetSystemTimeLow() - t0);

		if (result < 0) {
			failed++;
		}
		if (s.codec == AUDIOCODEC_MP3) {
			if (ctx.srcBytesRead <= 0) {
				break;
			}
			pos += ctx.srcBytesRead;
		} else if (s.codec != AUDIOCODEC_AAC) {
			pos += len;
		}
		if (frame >= WARMUP) {
			sum += us;
			timed++;
			if (us < minUs) minUs = us;
			if (us > maxUs) maxUs = us;
		}
	}
	sceAudiocodecReleaseEDRAM(&ctx);
	free(file);

	if (timed == 0) {
		printf("%-14s no frames decoded\n", s.name);
		return;
	}
	printf("%-14s %6d %6d %6d %6d %6d\n", s.name, minUs, sum / timed, maxUs, (int)ctx.dstBytesWritten, failed);
}

// The calls around decoding also go to the ME, and block while it answers.
static void timeCalls() {
	static const int codecs[] = { AUDIOCODEC_AT3PLUS, AUDIOCODEC_AT3, AUDIOCODEC_MP3, AUDIOCODEC_AAC };
	static const char *names[] = { "at3+", "at3", "mp3", "aac" };
	printf("%-6s %9s %8s %6s %8s %7s\n", "codec", "needmem", "getedram", "init", "release", "getinfo");
	int i, r;
	for (i = 0; i < 4; i++) {
		int best[5] = { 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF };
		for (r = 0; r < 8; r++) {
			memset(&ctx, 0, sizeof(ctx));
			if (codecs[i] == AUDIOCODEC_AT3PLUS) {
				ctx.fmt.at3.formatByte1 = 0x28;
				ctx.fmt.at3.formatByte2 = 0x5c;
			} else if (codecs[i] == AUDIOCODEC_AT3) {
				*(u32 *)ctx.fmt.raw = 0x04;
			} else if (codecs[i] == AUDIOCODEC_AAC) {
				*(u32 *)ctx.fmt.raw = 44100;
			}
			u32 t[6];
			t[0] = sceKernelGetSystemTimeLow();
			sceAudiocodecCheckNeedMem(&ctx, codecs[i]);
			t[1] = sceKernelGetSystemTimeLow();
			sceAudiocodecGetEDRAM(&ctx, codecs[i]);
			t[2] = sceKernelGetSystemTimeLow();
			sceAudiocodecInit(&ctx, codecs[i]);
			t[3] = sceKernelGetSystemTimeLow();
			// Only MP3 has anything to find out; the call goes to the ME either way.
			ctx.inBuf = frameBuf;
			sceAudiocodecGetInfo(&ctx, codecs[i]);
			t[4] = sceKernelGetSystemTimeLow();
			sceAudiocodecReleaseEDRAM(&ctx);
			t[5] = sceKernelGetSystemTimeLow();
			int us[5] = { (int)(t[1] - t[0]), (int)(t[2] - t[1]), (int)(t[3] - t[2]), (int)(t[5] - t[4]), (int)(t[4] - t[3]) };
			int j;
			for (j = 0; j < 5; j++) {
				if (us[j] < best[j]) best[j] = us[j];
			}
		}
		printf("%-6s %9d %8d %6d %8d %7d\n", names[i], best[0], best[1], best[2], best[3], best[4]);
	}
}

// Decodes that fail, and InitMono.
static void timeFailures() {
	static u8 junk[0x800] __attribute__((aligned(64)));
	int i;
	for (i = 0; i < (int)sizeof(junk); i++) {
		junk[i] = (u8)((i * 2654435761u) >> 13);
	}
	sceKernelDcacheWritebackRange(junk, sizeof(junk));
	int best[4] = { 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF };
	int r;
	for (r = 0; r < 8; r++) {
		u32 t0, us;
		// Atrac3+, raw frames: junk fails in the bitstream (err 0x20a).
		memset(&ctx, 0, sizeof(ctx));
		ctx.fmt.at3.formatByte1 = 0x28;
		ctx.fmt.at3.formatByte2 = 0x2e;
		ctx.inited = 1;
		sceAudiocodecCheckNeedMem(&ctx, AUDIOCODEC_AT3PLUS);
		sceAudiocodecGetEDRAM(&ctx, AUDIOCODEC_AT3PLUS);
		sceAudiocodecInit(&ctx, AUDIOCODEC_AT3PLUS);
		ctx.inBuf = junk;
		ctx.outBuf = pcm;
		t0 = sceKernelGetSystemTimeLow();
		sceAudiocodecDecode(&ctx, AUDIOCODEC_AT3PLUS);
		us = sceKernelGetSystemTimeLow() - t0;
		if ((int)us < best[0]) best[0] = us;
		// Headered frames: no sync word (err 0x211).
		ctx.fmt.at3.at3Related = 1;
		t0 = sceKernelGetSystemTimeLow();
		sceAudiocodecDecode(&ctx, AUDIOCODEC_AT3PLUS);
		us = sceKernelGetSystemTimeLow() - t0;
		if ((int)us < best[1]) best[1] = us;
		sceAudiocodecReleaseEDRAM(&ctx);

		// Atrac3 junk (err 0x182).
		memset(&ctx, 0, sizeof(ctx));
		*(u32 *)ctx.fmt.raw = 0x04;
		sceAudiocodecCheckNeedMem(&ctx, AUDIOCODEC_AT3);
		sceAudiocodecGetEDRAM(&ctx, AUDIOCODEC_AT3);
		sceAudiocodecInit(&ctx, AUDIOCODEC_AT3);
		ctx.inBuf = junk;
		ctx.outBuf = pcm;
		t0 = sceKernelGetSystemTimeLow();
		sceAudiocodecDecode(&ctx, AUDIOCODEC_AT3);
		us = sceKernelGetSystemTimeLow() - t0;
		if ((int)us < best[2]) best[2] = us;
		sceAudiocodecReleaseEDRAM(&ctx);

		// InitMono, as libatrac3plus.prx uses it for the MOut functions.
		memset(&ctx, 0, sizeof(ctx));
		ctx.fmt.at3.formatByte1 = 0x24;
		ctx.fmt.at3.formatByte2 = 0x5c;
		ctx.inited = 1;
		sceAudiocodecCheckNeedMem(&ctx, AUDIOCODEC_AT3PLUS);
		sceAudiocodecGetEDRAM(&ctx, AUDIOCODEC_AT3PLUS);
		t0 = sceKernelGetSystemTimeLow();
		sceAudiocodecInitMono(&ctx, AUDIOCODEC_AT3PLUS);
		us = sceKernelGetSystemTimeLow() - t0;
		if ((int)us < best[3]) best[3] = us;
		sceAudiocodecReleaseEDRAM(&ctx);
	}
	printf("failed decode: at3+ junk %d, at3+ no sync %d, at3 junk %d; at3+ InitMono %d\n", best[0], best[1], best[2], best[3]);
}

extern "C" int main(int argc, char *argv[]) {
	HAS_DISPLAY = 0;
	if (sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC) < 0) {
		printf("Could not load avcodec\n");
		return 1;
	}

	static const int clocks[2][3] = { { 222, 222, 111 }, { 333, 333, 166 } };
	int k, i;
	for (k = 0; k < 2; k++) {
		scePowerSetClockFrequency(clocks[k][0], clocks[k][1], clocks[k][2]);
		printf("clocks: cpu %d, bus %d\n", scePowerGetCpuClockFrequencyInt(), scePowerGetBusClockFrequencyInt());
		printf("%-14s %6s %6s %6s %6s %6s\n", "stream", "min us", "avg us", "max us", "bytes", "failed");
		for (i = 0; i < (int)(sizeof(streams) / sizeof(streams[0])); i++) {
			runStream(streams[i]);
		}
		timeCalls();
		timeFailures();
	}
	scePowerSetClockFrequency(222, 222, 111);

	sceUtilityUnloadModule(PSP_MODULE_AV_AVCODEC);
	return 0;
}
