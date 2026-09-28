#include <common.h>

#include <psputility.h>
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "audiocodec.h"

// sceAudiocodec directly: the memory check, the ME's EDRAM, init, decoding real frames of each
// codec, and what the decoder reports for bad input. Decoded samples aren't compared (the decoders
// aren't bit exact against each other), only what the calls report.

static SceAudiocodecCodec ctx __attribute__((aligned(64)));
static short pcm[2048 * 2 * 2] __attribute__((aligned(64)));
static u8 frameBuf[0x800] __attribute__((aligned(64)));

// sample.at3: Atrac3plus stereo, 376-byte frames, format bytes 28 2e in the fmt chunk, data from 0x60.
static const int AT3PLUS_DATA = 0x60;
static const int AT3PLUS_FRAME = 376;
// test_mono.at3: Atrac3, 152-byte frames, no joint stereo, data from 0x50.
static const int AT3_DATA = 0x50;
static const int AT3_FRAME = 152;

static const char *codecName(int codec) {
	switch (codec) {
	case AUDIOCODEC_AT3PLUS: return "at3+";
	case AUDIOCODEC_AT3: return "at3";
	case AUDIOCODEC_MP3: return "mp3";
	case AUDIOCODEC_AAC: return "aac";
	default: return "?";
	}
}

// Sets up the context the way the library that normally drives each codec does.
static void resetCtx(int codec) {
	memset(&ctx, 0, sizeof(ctx));
	switch (codec) {
	case AUDIOCODEC_AT3PLUS:
		// As mpeg.prx sets up its Atrac3plus decoder (0880b1ac, format bytes from its table at
		// 0880bf74): the largest stereo frame, and inited and at3Related set. With format bytes
		// of zero, CheckNeedMem fails.
		ctx.fmt.at3.formatByte1 = 0x28;
		ctx.fmt.at3.formatByte2 = 0x5c;
		ctx.fmt.at3.at3Related = 1;
		ctx.inited = 1;
		break;
	case AUDIOCODEC_AT3:
		// libatrac3plus.prx's SetData (0880645c) stores a parameter from its table keyed by frame
		// size and joint stereo: 0x0F for 152-byte frames.
		*(u32 *)ctx.fmt.raw = 0x0F;
		break;
	case AUDIOCODEC_AAC:
		// As libmp4.prx: the sample rate.
		*(u32 *)ctx.fmt.raw = 44100;
		break;
	}
}

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
	// The Media Engine reads main memory directly, not through the CPU's cache.
	sceKernelDcacheWritebackRange(data, *size);
	return data;
}

static int countNonZero(int bytes) {
	int n = 0, i;
	for (i = 0; i < bytes / 2; i++) {
		if (pcm[i] != 0) {
			n++;
		}
	}
	return n;
}

static int setupDecoder(int codec) {
	sceAudiocodecCheckNeedMem(&ctx, codec);
	sceAudiocodecGetEDRAM(&ctx, codec);
	return sceAudiocodecInit(&ctx, codec);
}

static int decodeOne(int codec, const void *in, const char *label) {
	ctx.inBuf = (void *)in;
	ctx.outBuf = pcm;
	memset(pcm, 0, sizeof(pcm));
	sceKernelDcacheWritebackInvalidateRange(pcm, sizeof(pcm));
	int result = sceAudiocodecDecode(&ctx, codec);
	sceKernelDcacheInvalidateRange(pcm, sizeof(pcm));
	checkpoint("    %s: %08x (err %08x, read %d, wrote %d, output %s)", label, result, (int)ctx.err, (int)ctx.srcBytesRead, (int)ctx.dstBytesWritten, countNonZero(ctx.dstBytesWritten) ? "nonzero" : "silent");
	return result;
}

// Copies a frame into frameBuf, optionally behind a PSMF-style 8-byte header.
static const u8 *prepFrame(const u8 *src, int len, const u8 *header) {
	int offset = 0;
	if (header) {
		memcpy(frameBuf, header, 8);
		offset = 8;
	}
	memcpy(frameBuf + offset, src, len);
	sceKernelDcacheWritebackRange(frameBuf, sizeof(frameBuf));
	return frameBuf;
}

static void testCheckNeedMem() {
	checkpointNext("CheckNeedMem:");
	static const int codecs[] = { 0x0FFF, 0x1000, 0x1001, 0x1002, 0x1003, 0x1004, 0x1005, 0x1006 };
	int i;
	for (i = 0; i < (int)(sizeof(codecs) / sizeof(codecs[0])); i++) {
		memset(&ctx, 0, sizeof(ctx));
		int result = sceAudiocodecCheckNeedMem(&ctx, codecs[i]);
		checkpoint("  %04x, zeroed context: %08x (err %08x, neededMem %08x)", codecs[i], result, (int)ctx.err, (int)ctx.neededMem);
	}
	for (i = 1; i <= 4; i++) {
		resetCtx(codecs[i]);
		int result = sceAudiocodecCheckNeedMem(&ctx, codecs[i]);
		checkpoint("  %s, set up: %08x (err %08x, neededMem %08x)", codecName(codecs[i]), result, (int)ctx.err, (int)ctx.neededMem);
	}
	resetCtx(AUDIOCODEC_AT3PLUS);
	ctx.fmt.at3.formatByte1 = 0x28;
	ctx.fmt.at3.formatByte2 = 0x2e;
	int result = sceAudiocodecCheckNeedMem(&ctx, AUDIOCODEC_AT3PLUS);
	checkpoint("  at3+, format 28/2e: %08x (err %08x, neededMem %08x)", result, (int)ctx.err, (int)ctx.neededMem);
	resetCtx(AUDIOCODEC_AT3PLUS);
	ctx.inited = 0;
	ctx.fmt.at3.at3Related = 0;
	result = sceAudiocodecCheckNeedMem(&ctx, AUDIOCODEC_AT3PLUS);
	checkpoint("  at3+, not inited: %08x (err %08x, neededMem %08x)", result, (int)ctx.err, (int)ctx.neededMem);
	resetCtx(AUDIOCODEC_AAC);
	*(u32 *)ctx.fmt.raw = 48000;
	result = sceAudiocodecCheckNeedMem(&ctx, AUDIOCODEC_AAC);
	checkpoint("  aac, 48000: %08x (err %08x, neededMem %08x)", result, (int)ctx.err, (int)ctx.neededMem);
}

static void testEdram() {
	checkpointNext("EDRAM:");
	resetCtx(AUDIOCODEC_AT3PLUS);
	sceAudiocodecCheckNeedMem(&ctx, AUDIOCODEC_AT3PLUS);
	int result = sceAudiocodecGetEDRAM(&ctx, AUDIOCODEC_AT3PLUS);
	checkpoint("  GetEDRAM: %08x (address set: %s)", result, ctx.edramAddr != 0 ? "yes" : "no");
	u32 first = ctx.edramAddr;
	result = sceAudiocodecReleaseEDRAM(&ctx);
	checkpoint("  ReleaseEDRAM: %08x (address cleared: %s)", result, ctx.edramAddr == 0 ? "yes" : "no");
	result = sceAudiocodecGetEDRAM(&ctx, AUDIOCODEC_AT3PLUS);
	checkpoint("  GetEDRAM again: %08x (same address: %s)", result, ctx.edramAddr == first ? "yes" : "no");
	checkpoint("  ReleaseEDRAM: %08x", sceAudiocodecReleaseEDRAM(&ctx));
}

static void testInit() {
	checkpointNext("Init:");
	static const int codecs[] = { 0x1000, 0x1001, 0x1002, 0x1003 };
	int i;
	for (i = 0; i < (int)(sizeof(codecs) / sizeof(codecs[0])); i++) {
		resetCtx(codecs[i]);
		sceAudiocodecCheckNeedMem(&ctx, codecs[i]);
		int edram = sceAudiocodecGetEDRAM(&ctx, codecs[i]);
		int result = sceAudiocodecInit(&ctx, codecs[i]);
		checkpoint("  %s: GetEDRAM %08x, Init %08x (err %08x, magic %08x)", codecName(codecs[i]), edram, result, (int)ctx.err, (int)ctx.magic);
		sceAudiocodecReleaseEDRAM(&ctx);
	}

	resetCtx(AUDIOCODEC_AT3);
	*(u32 *)ctx.fmt.raw = 0x5c28;
	checkpoint("  at3, at3+ format bytes: Init %08x (err %08x)", setupDecoder(AUDIOCODEC_AT3), (int)ctx.err);
	sceAudiocodecReleaseEDRAM(&ctx);
	resetCtx(AUDIOCODEC_AT3);
	*(u32 *)ctx.fmt.raw = 0x10;
	checkpoint("  at3, parameter 0x10: Init %08x (err %08x)", setupDecoder(AUDIOCODEC_AT3), (int)ctx.err);
	sceAudiocodecReleaseEDRAM(&ctx);
	resetCtx(AUDIOCODEC_AAC);
	*(u32 *)ctx.fmt.raw = 0;
	checkpoint("  aac, sample rate 0: Init %08x (err %08x)", setupDecoder(AUDIOCODEC_AAC), (int)ctx.err);
	sceAudiocodecReleaseEDRAM(&ctx);
	resetCtx(AUDIOCODEC_AAC);
	*(u32 *)ctx.fmt.raw = 12345;
	checkpoint("  aac, sample rate 12345: Init %08x (err %08x)", setupDecoder(AUDIOCODEC_AAC), (int)ctx.err);
	sceAudiocodecReleaseEDRAM(&ctx);
	resetCtx(AUDIOCODEC_AAC);
	*(u32 *)ctx.fmt.raw = 22050;
	checkpoint("  aac, sample rate 22050: Init %08x (err %08x)", setupDecoder(AUDIOCODEC_AAC), (int)ctx.err);
	sceAudiocodecReleaseEDRAM(&ctx);
}

// Atrac3plus frames arrive two ways: libatrac3plus.prx strips the 8-byte header (0F D0 + format)
// and zeroes at3Related, mpeg.prx leaves the header on and at3Related at 1.
static void decodeAt3plusVariant(const u8 *file, int zeroAt3Related, int withHeader, int libatracInit = 0) {
	resetCtx(AUDIOCODEC_AT3PLUS);
	sceAudiocodecCheckNeedMem(&ctx, AUDIOCODEC_AT3PLUS);
	sceAudiocodecGetEDRAM(&ctx, AUDIOCODEC_AT3PLUS);
	ctx.fmt.at3.formatByte1 = file[0x3e];
	ctx.fmt.at3.formatByte2 = file[0x3f];
	if (libatracInit) {
		// As libatrac3plus.prx's sceAtracLowLevelInitDecoder (08806804) does before Init.
		ctx.fmt.at3.unk2a = 0;
		ctx.fmt.at3.unk2b = 0;
		ctx.fmt.at3.unk2c = 0;
		ctx.fmt.at3.at3Related = 0;
		memset(ctx.fmt.raw + 0x10, 0, 4);  // 0x38
	}
	int result = sceAudiocodecInit(&ctx, AUDIOCODEC_AT3PLUS);
	checkpoint("  %s%s at3Related, %s header: Init %08x (err %08x)", libatracInit ? "libatrac init, " : "", zeroAt3Related ? "zeroed" : "kept", withHeader ? "with" : "no", result, (int)ctx.err);
	const u8 header[8] = { 0x0F, 0xD0, file[0x3e], file[0x3f], 0, 0, 0, 0 };
	int frame;
	for (frame = 0; frame < 3; frame++) {
		const u8 *src = file + AT3PLUS_DATA + frame * AT3PLUS_FRAME;
		if (zeroAt3Related) {
			ctx.fmt.at3.at3Related = 0;
		}
		char label[16];
		snprintf(label, sizeof(label), "frame %d", frame);
		decodeOne(AUDIOCODEC_AT3PLUS, withHeader ? prepFrame(src, AT3PLUS_FRAME, header) : src, label);
	}
	sceAudiocodecReleaseEDRAM(&ctx);
}

// A raw-frame decoder (as libatrac3plus.prx sets one up) with format bytes b1/b2.
static void startAt3plusRaw(const char *title, u8 b1, u8 b2) {
	resetCtx(AUDIOCODEC_AT3PLUS);
	ctx.fmt.at3.formatByte1 = b1;
	ctx.fmt.at3.formatByte2 = b2;
	ctx.fmt.at3.at3Related = 0;
	checkpoint("  %s: Init %08x (err %08x)", title, setupDecoder(AUDIOCODEC_AT3PLUS), (int)ctx.err);
}

static void testAt3plusErrors(const u8 *file) {
	checkpointNext("At3+ bad input:");
	const u8 *f0 = file + AT3PLUS_DATA;
	const u8 *f1 = f0 + AT3PLUS_FRAME, *f2 = f1 + AT3PLUS_FRAME, *f3 = f2 + AT3PLUS_FRAME;
	static u8 junk[0x800] __attribute__((aligned(64)));
	int i;
	for (i = 0; i < (int)sizeof(junk); i++) {
		junk[i] = (u8)((i * 2654435761u) >> 13);
	}
	sceKernelDcacheWritebackRange(junk, sizeof(junk));
	static u8 zeros[0x800] __attribute__((aligned(64)));
	sceKernelDcacheWritebackRange(zeros, sizeof(zeros));

	startAt3plusRaw("junk mid-stream", file[0x3e], file[0x3f]);
	decodeOne(AUDIOCODEC_AT3PLUS, f0, "frame 0");
	decodeOne(AUDIOCODEC_AT3PLUS, f1, "frame 1");
	decodeOne(AUDIOCODEC_AT3PLUS, junk, "junk");
	decodeOne(AUDIOCODEC_AT3PLUS, junk, "junk");
	decodeOne(AUDIOCODEC_AT3PLUS, f2, "frame 2");
	decodeOne(AUDIOCODEC_AT3PLUS, f3, "frame 3");
	sceAudiocodecReleaseEDRAM(&ctx);

	startAt3plusRaw("junk first", file[0x3e], file[0x3f]);
	decodeOne(AUDIOCODEC_AT3PLUS, junk, "junk");
	decodeOne(AUDIOCODEC_AT3PLUS, f0, "frame 0");
	decodeOne(AUDIOCODEC_AT3PLUS, f1, "frame 1");
	decodeOne(AUDIOCODEC_AT3PLUS, f2, "frame 2");
	sceAudiocodecReleaseEDRAM(&ctx);

	startAt3plusRaw("zeros", file[0x3e], file[0x3f]);
	decodeOne(AUDIOCODEC_AT3PLUS, zeros, "zeros");
	decodeOne(AUDIOCODEC_AT3PLUS, zeros, "zeros");
	decodeOne(AUDIOCODEC_AT3PLUS, f0, "frame 0");
	decodeOne(AUDIOCODEC_AT3PLUS, f1, "frame 1");
	sceAudiocodecReleaseEDRAM(&ctx);

	// Not tested here, as they depend on details of the bitstream an emulator can't match: a frame
	// read from 4 bytes in fails with err 0x20a and then 0x208, and a context frame size too small
	// for the data (28 22 on a 28 2e stream) fails every frame with err 0x214.

	startAt3plusRaw("larger frame size (28 5c)", 0x28, 0x5c);
	decodeOne(AUDIOCODEC_AT3PLUS, f0, "frame 0");
	decodeOne(AUDIOCODEC_AT3PLUS, f1, "frame 1");
	sceAudiocodecReleaseEDRAM(&ctx);

	// Decoding after this fails with err 0x214.
	startAt3plusRaw("no channels (20 2e)", 0x20, 0x2e);
	sceAudiocodecReleaseEDRAM(&ctx);

	// Headered frames, as mpeg.prx passes them.
	resetCtx(AUDIOCODEC_AT3PLUS);
	checkpoint("  headers: Init %08x (err %08x)", setupDecoder(AUDIOCODEC_AT3PLUS), (int)ctx.err);
	const u8 good[8] = { 0x0F, 0xD0, file[0x3e], file[0x3f], 0, 0, 0, 0 };
	const u8 badSync[8] = { 0x0F, 0xD1, file[0x3e], file[0x3f], 0, 0, 0, 0 };
	const u8 bigger[8] = { 0x0F, 0xD0, 0x28, 0x5c, 0, 0, 0, 0 };
	const u8 mono[8] = { 0x0F, 0xD0, 0x20, file[0x3f], 0, 0, 0, 0 };
	const u8 highBits[8] = { 0x0F, 0xD0, (u8)(file[0x3e] | 1), file[0x3f], 0, 0, 0, 0 };
	decodeOne(AUDIOCODEC_AT3PLUS, prepFrame(f0, AT3PLUS_FRAME, good), "good header");
	decodeOne(AUDIOCODEC_AT3PLUS, prepFrame(f1, AT3PLUS_FRAME, badSync), "sync 0FD1");
	decodeOne(AUDIOCODEC_AT3PLUS, prepFrame(f1, AT3PLUS_FRAME, bigger), "header says 28 5c");
	decodeOne(AUDIOCODEC_AT3PLUS, prepFrame(f1, AT3PLUS_FRAME, mono), "header says mono");
	decodeOne(AUDIOCODEC_AT3PLUS, prepFrame(f1, AT3PLUS_FRAME, highBits), "header size high bit");
	decodeOne(AUDIOCODEC_AT3PLUS, prepFrame(f1, AT3PLUS_FRAME, good), "good header");
	decodeOne(AUDIOCODEC_AT3PLUS, prepFrame(f2, AT3PLUS_FRAME, good), "good header");
	sceAudiocodecReleaseEDRAM(&ctx);
}

static void testDecodeAt3plus() {
	checkpointNext("Decode at3+:");
	int size = 0;
	u8 *file = loadFile("../atrac/sample.at3", &size);
	if (!file) {
		return;
	}
	decodeAt3plusVariant(file, 1, 0);
	decodeAt3plusVariant(file, 0, 0);
	decodeAt3plusVariant(file, 0, 1);
	decodeAt3plusVariant(file, 1, 1);
	decodeAt3plusVariant(file, 1, 0, 1);
	testAt3plusErrors(file);
	free(file);
}

static void testDecodeAt3() {
	checkpointNext("Decode at3:");
	int size = 0;
	u8 *file = loadFile("../atrac/test_mono.at3", &size);
	if (!file) {
		return;
	}
	resetCtx(AUDIOCODEC_AT3);
	checkpoint("  Init: %08x (err %08x)", setupDecoder(AUDIOCODEC_AT3), (int)ctx.err);
	int frame;
	for (frame = 0; frame < 4; frame++) {
		char label[16];
		snprintf(label, sizeof(label), "frame %d", frame);
		decodeOne(AUDIOCODEC_AT3, file + AT3_DATA + frame * AT3_FRAME, label);
	}
	static u8 zeros[0x200] __attribute__((aligned(64)));
	sceKernelDcacheWritebackRange(zeros, sizeof(zeros));
	decodeOne(AUDIOCODEC_AT3, zeros, "zeros");
	decodeOne(AUDIOCODEC_AT3, file + AT3_DATA + 4 * AT3_FRAME, "frame 4");
	sceAudiocodecReleaseEDRAM(&ctx);
	free(file);
}

static void testDecodeAac() {
	checkpointNext("Decode aac:");
	int size = 0;
	u8 *file = loadFile("aac_128k.aac", &size);
	if (!file) {
		return;
	}
	// Raw access units as libmp4.prx passes them: skip each ADTS header. (Setting the byte at 0x2c,
	// which makes avcodec.prx size the input 9 bytes larger, fails Init with 807f00ff.)
	resetCtx(AUDIOCODEC_AAC);
	checkpoint("  Init %08x (err %08x)", setupDecoder(AUDIOCODEC_AAC), (int)ctx.err);
	int pos = 0, frame;
	for (frame = 0; frame < 4; frame++) {
		int len = ((file[pos + 3] & 3) << 11) | (file[pos + 4] << 3) | (file[pos + 5] >> 5);
		char label[16];
		snprintf(label, sizeof(label), "frame %d", frame);
		decodeOne(AUDIOCODEC_AAC, file + pos + 7, label);
		pos += len;
	}
	sceAudiocodecReleaseEDRAM(&ctx);
	free(file);
}

static void testDecodeMp3() {
	checkpointNext("Decode mp3:");
	int size = 0;
	u8 *file = loadFile("../mp3/sample.mp3", &size);
	if (!file) {
		return;
	}
	resetCtx(AUDIOCODEC_MP3);
	sceAudiocodecCheckNeedMem(&ctx, AUDIOCODEC_MP3);
	sceAudiocodecGetEDRAM(&ctx, AUDIOCODEC_MP3);
	checkpoint("  Init: %08x (version %d)", sceAudiocodecInit(&ctx, AUDIOCODEC_MP3), (int)ctx.fmt.mp3.version);
	// What libmp3.prx sets up once: the largest possible frame.
	ctx.fmt.mp3.maxFrameBytes = 0x5A1;
	int pos = 0, frame;
	for (frame = 0; frame < 4 && pos < size; frame++) {
		char label[16];
		snprintf(label, sizeof(label), "frame %d", frame);
		int result = decodeOne(AUDIOCODEC_MP3, file + pos, label);
		if (result < 0 || ctx.srcBytesRead <= 0) {
			break;
		}
		pos += ctx.srcBytesRead;
	}
	checkpoint("  GetInfo: %08x (version %d, bitrateIndex %d, sampleRateIndex %d, channelConfig %d)", sceAudiocodecGetInfo(&ctx, AUDIOCODEC_MP3), (int)ctx.fmt.mp3.version, (int)ctx.fmt.mp3.bitrateIndex, (int)ctx.fmt.mp3.sampleRateIndex, (int)ctx.fmt.mp3.channelConfig);
	sceAudiocodecReleaseEDRAM(&ctx);
	free(file);
}

extern "C" int main(int argc, char *argv[]) {
	if (sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC) < 0) {
		printf("Could not load avcodec\n");
		return 1;
	}

	testCheckNeedMem();
	testEdram();
	testInit();
	testDecodeMp3();
	testDecodeAt3();
	testDecodeAac();
	// Last: bad input is the likeliest thing here to upset the ME.
	testDecodeAt3plus();

	sceUtilityUnloadModule(PSP_MODULE_AV_AVCODEC);
	return 0;
}
