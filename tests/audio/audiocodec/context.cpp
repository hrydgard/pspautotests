#include <common.h>

#include <psputility.h>
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "audiocodec.h"

// What each sceAudiocodec call writes into the context, for every codec: after each call, the
// words that changed ("+offset old->new", leaving out the buffer pointers). mp3_mono_64k.mp3 is a
// 440Hz sine, made with ffmpeg -f lavfi -i sine=frequency=440:duration=2 -ac 1 -c:a libmp3lame -b:a 64k.

extern "C" int sceAudiocodecGetBlockSize(SceAudiocodecCodec *ctx, int codec, u32 *out);

static SceAudiocodecCodec ctx __attribute__((aligned(64)));
static short pcm[4096 * 2] __attribute__((aligned(64)));
static u8 frame[0x1000] __attribute__((aligned(64)));
static u32 snap[32];

static u8 *loadFile(const char *path, int *size) {
	SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
	if (fd < 0) {
		checkpoint("Can't open %s", path);
		return NULL;
	}
	*size = sceIoLseek32(fd, 0, PSP_SEEK_END);
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	u8 *data = (u8 *)memalign(64, *size);
	sceIoRead(fd, data, *size);
	sceIoClose(fd);
	return data;
}

static void take() {
	memcpy(snap, &ctx, sizeof(snap));
}

static void step(const char *name, int r) {
	sceKernelDcacheWritebackInvalidateRange(&ctx, sizeof(ctx));
	char line[512];
	int len = snprintf(line, sizeof(line), "  %-14s %08x:", name, r);
	const u32 *w = (const u32 *)&ctx;
	for (int i = 0; i < 32; ++i) {
		if (i == 6 || i == 8) {
			continue;
		}
		if (w[i] != snap[i] && len < (int)sizeof(line) - 32) {
			len += snprintf(line + len, sizeof(line) - len, " +%02x %x->%x", i * 4, (unsigned)snap[i], (unsigned)w[i]);
		}
	}
	checkpoint("%s", line);
	take();
}

static void begin(const char *title) {
	checkpointNext(title);
	memset(&ctx, 0, sizeof(ctx));
	take();
}

static void w32(int off, u32 v) {
	*(u32 *)((u8 *)&ctx + off) = v;
	take();
}

static void setup(int codec) {
	take();
	step("CheckNeedMem", sceAudiocodecCheckNeedMem(&ctx, codec));
	step("GetEDRAM", sceAudiocodecGetEDRAM(&ctx, codec));
}

static void decode(int codec, const u8 *src, int len, const char *name) {
	memcpy(frame, src, len);
	sceKernelDcacheWritebackRange(frame, sizeof(frame));
	ctx.inBuf = frame;
	ctx.outBuf = pcm;
	take();
	step(name, sceAudiocodecDecode(&ctx, codec));
}

static void blockSize(int codec) {
	u32 out = 0xDEADBEEF;
	int r = sceAudiocodecGetBlockSize(&ctx, codec, &out);
	checkpoint("  GetOutputBytes %08x: %x", r, (unsigned)out);
	take();
}

static void release() {
	take();
	step("ReleaseEDRAM", sceAudiocodecReleaseEDRAM(&ctx));
}

static void at3plusInit(const char *title, u8 b1, u8 b2, int inited, bool mono) {
	begin(title);
	ctx.fmt.at3.formatByte1 = b1;
	ctx.fmt.at3.formatByte2 = b2;
	ctx.inited = inited;
	ctx.fmt.at3.at3Related = 1;
	setup(0x1000);
	step(mono ? "InitMono" : "Init", mono ? sceAudiocodecInitMono(&ctx, 0x1000) : sceAudiocodecInit(&ctx, 0x1000));
	blockSize(0x1000);
	sceAudiocodecReleaseEDRAM(&ctx);
}

static int skipId3(const u8 *f) {
	if (f[0] == 'I' && f[1] == 'D' && f[2] == '3') {
		return 10 + ((f[6] & 0x7f) << 21 | (f[7] & 0x7f) << 14 | (f[8] & 0x7f) << 7 | (f[9] & 0x7f));
	}
	return 0;
}

static void testMp3(const char *name) {
	int size = 0;
	u8 *f = loadFile(name, &size);
	if (!f) {
		return;
	}
	int pos = skipId3(f);
	begin(name);
	setup(0x1002);
	step("Init", sceAudiocodecInit(&ctx, 0x1002));
	w32(0x28, 0x5A1);
	memcpy(frame, f + pos, 0x5A1);
	sceKernelDcacheWritebackRange(frame, sizeof(frame));
	ctx.inBuf = frame;
	take();
	step("GetInfo", sceAudiocodecGetInfo(&ctx, 0x1002));
	blockSize(0x1002);
	for (int i = 0; i < 2; ++i) {
		decode(0x1002, f + pos, 0x5A1, "Decode");
		pos += ctx.srcBytesRead;
	}
	release();
	free(f);
}

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	int ps = 0, ms = 0, ss = 0, aa = 0;
	u8 *plus = loadFile("../atrac/sample.at3", &ps);
	u8 *mono = loadFile("atrac3_c0_mono.at3", &ms);
	u8 *stereo = loadFile("atrac3_180_stereo.raw", &ss);
	u8 *aac = loadFile("aac_128k.aac", &aa);
	if (!plus || !mono || !stereo || !aac) {
		return 1;
	}
	mono += 0x4C;

	// Atrac3+: Init wants inited to be exactly 1; at3Related doesn't matter.
	at3plusInit("Atrac3+ 28 2e, inited 0", 0x28, 0x2e, 0, false);
	at3plusInit("Atrac3+ 28 2e, inited 2", 0x28, 0x2e, 2, false);
	// The frame size, and the bitrate Init works out from it.
	static const u8 b2s[] = { 0x0b, 0x1f, 0x2e, 0x45, 0x5c, 0xff };
	for (u8 b2 : b2s) {
		char t[64];
		snprintf(t, sizeof(t), "Atrac3+ 28 %02x", b2);
		at3plusInit(t, 0x28, b2, 1, false);
	}
	// Channels (and neededMem and the output channels by them), the rate, the frame size's high bits.
	static const u8 b1s[] = { 0x20, 0x24, 0x2c, 0x30, 0x34, 0x38, 0x3c, 0x29, 0x2a, 0x08, 0x48, 0x68 };
	for (u8 b1 : b1s) {
		char t[64];
		snprintf(t, sizeof(t), "Atrac3+ %02x 2e", b1);
		at3plusInit(t, b1, 0x2e, 1, false);
	}
	at3plusInit("Atrac3+ 24 2e, InitMono", 0x24, 0x2e, 1, true);
	at3plusInit("Atrac3+ 28 2e, InitMono", 0x28, 0x2e, 1, true);

	// Atrac3+ decoding, raw frames as libatrac3plus.prx passes them. The first frame gives no output.
	begin("Atrac3+ decode");
	ctx.fmt.at3.formatByte1 = plus[0x3e];
	ctx.fmt.at3.formatByte2 = plus[0x3f];
	ctx.inited = 1;
	setup(0x1000);
	step("Init", sceAudiocodecInit(&ctx, 0x1000));
	for (int i = 0; i < 3; ++i) {
		ctx.fmt.at3.at3Related = 0;
		decode(0x1000, plus + 0x60 + i * 376, 376, "Decode");
	}
	// Asking for one output channel afterwards (the samples are a mixdown, not checked here).
	w32(0x48, 1);
	decode(0x1000, plus + 0x60 + 3 * 376, 376, "Decode");
	blockSize(0x1000);
	release();

	// Atrac3: Init writes the per-channel frame size, and the output channels for mono layouts
	// only. Decode goes by the parameter, not by what's written back.
	begin("Atrac3 0e");
	w32(0x28, 0x0E);
	setup(0x1001);
	step("Init", sceAudiocodecInit(&ctx, 0x1001));
	blockSize(0x1001);
	w32(0x30, 0x98);
	w32(0x2c, 22050);
	decode(0x1001, mono + 0xC0 * 10, 0xC0, "Decode");
	release();
	begin("Atrac3 04");
	w32(0x28, 0x04);
	setup(0x1001);
	step("Init", sceAudiocodecInit(&ctx, 0x1001));
	decode(0x1001, stereo, 0x180, "Decode");
	release();
	static const u32 params[] = { 0x00, 0x03, 0x09, 0x0c, 0x0d };
	for (u32 param : params) {
		char t[32];
		snprintf(t, sizeof(t), "Atrac3 %02x", (int)param);
		begin(t);
		w32(0x28, param);
		setup(0x1001);
		step("Init", sceAudiocodecInit(&ctx, 0x1001));
		sceAudiocodecReleaseEDRAM(&ctx);
	}

	// MP3: Init marks the version unknown; GetInfo without a frame fails and fills in -1.
	begin("MP3 without a frame");
	setup(0x1002);
	step("Init", sceAudiocodecInit(&ctx, 0x1002));
	blockSize(0x1002);
	w32(0x28, 0x5A1);
	step("GetInfo", sceAudiocodecGetInfo(&ctx, 0x1002));
	blockSize(0x1002);
	release();
	// With frames: GetInfo, and every decode, read the header into the context.
	testMp3("mp3_128k.mp3");
	testMp3("mp3_320k.mp3");
	testMp3("mp3_22khz_32k.mp3");
	testMp3("mp3_mono_64k.mp3");
	begin("MP3 InitMono");
	setup(0x1002);
	step("InitMono", sceAudiocodecInitMono(&ctx, 0x1002));
	sceAudiocodecReleaseEDRAM(&ctx);

	// AAC: the sample rate is checked against a list, and either flag byte fails Init.
	static const u32 rates[] = { 44100, 48000, 22050, 12000, 8000, 96000, 0, 12345 };
	for (u32 rate : rates) {
		char t[32];
		snprintf(t, sizeof(t), "AAC %u", (unsigned)rate);
		begin(t);
		w32(0x28, rate);
		setup(0x1003);
		step("Init", sceAudiocodecInit(&ctx, 0x1003));
		blockSize(0x1003);
		sceAudiocodecReleaseEDRAM(&ctx);
	}
	for (int b = 0; b < 2; ++b) {
		begin(b == 0 ? "AAC 0x2d set" : "AAC 0x2c set");
		w32(0x28, 44100);
		((u8 *)&ctx)[b == 0 ? 0x2d : 0x2c] = 1;
		setup(0x1003);
		step("Init", sceAudiocodecInit(&ctx, 0x1003));
		blockSize(0x1003);
		sceAudiocodecReleaseEDRAM(&ctx);
	}
	// Decoding raw access units; the first two give no output.
	begin("AAC decode");
	w32(0x28, 44100);
	setup(0x1003);
	step("Init", sceAudiocodecInit(&ctx, 0x1003));
	int pos = 0;
	for (int i = 0; i < 3; ++i) {
		int len = ((aac[pos + 3] & 3) << 11) | (aac[pos + 4] << 3) | (aac[pos + 5] >> 5);
		decode(0x1003, aac + pos + 7, len - 7, "Decode");
		pos += len;
	}
	release();
	return 0;
}
