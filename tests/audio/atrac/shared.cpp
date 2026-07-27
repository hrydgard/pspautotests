#include "shared.h"
#include <stdio.h>
#include <cstring>
#include <cstdlib>
#include <malloc.h>
#include <psputility.h>

void LoadAtrac() {
	int success = 0;
	success |= sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	success |= sceUtilityLoadModule(PSP_MODULE_AV_ATRAC3PLUS);
	if (success != 0) {
		printf("TEST FAILURE: unable to load sceAtrac.\n");
		exit(1);
	}
}

void UnloadAtrac() {
	sceUtilityUnloadModule(PSP_MODULE_AV_ATRAC3PLUS);
	sceUtilityUnloadModule(PSP_MODULE_AV_AVCODEC);
}

Atrac3File::Atrac3File(const char *filename) : data_(NULL), size_(0), pos_(0) {
	Reload(filename);
}

Atrac3File::~Atrac3File() {
	delete [] data_;
	data_ = NULL;
}

void Atrac3File::Reload(const char *filename) {
	delete [] data_;
	data_ = NULL;

	FILE *file = fopen(filename, "rb");
	if (file) {
		fseek(file, 0, SEEK_END);
		size_ = ftell(file);
		data_ = new u8[size_];
		memset(data_, 0, size_);

		fseek(file, 0, SEEK_SET);
		fread(data_, size_, 1, file);

		fclose(file);

		pos_ = 0;
	} else {
		printf("Failed to open file: %s\n", filename);
		size_ = 0;
	}
}

void Atrac3File::Require() {
	if (!IsValid()) {
		printf("TEST FAILURE: data_ == nullptr\n");
		exit(1);
	}
}

void CreateLoopedAtracFrom(Atrac3File &at3, Atrac3File &updated, int loopStart, int loopEnd, int numLoops) {
	// We need a bit of extra space to fake loop information.
	const u32 extraLoopInfoSize = 44 + 24;
	updated.Reset(at3.Size() + extraLoopInfoSize);
	u32 *data32 = (u32 *)updated.Data();

	// Tricksy stuff happening here.  Adding loop information.
	const u32 initialDataStart = 88;
	const u32 MAGIC_LOWER_SMPL = 0x6C706D73;
	memcpy(updated.Data(), at3.Data(), initialDataStart);
	// We need to add a sample chunk, and it's this long.
	data32[1] += 44 + 24;
	// Skip to where the sample chunk is going.
	data32 = (u32 *)(updated.Data() + initialDataStart);
	// Loop header.
	*data32++ = MAGIC_LOWER_SMPL;
	*data32++ = 36 + 24;
	*data32++ = 0; // manufacturer
	*data32++ = 0; // product
	*data32++ = 22676; // sample period
	*data32++ = 60; // midi unity note
	*data32++ = 0; // midi semi tone
	*data32++ = 0; // SMPTE offset format
	*data32++ = 0; // SMPTE offset
	*data32++ = numLoops; // num loops
	*data32++ = 0x18; // extra smpl bytes at end (seems incorrect, but found in data.)
					  // Loop info itself.
	*data32++ = 0; // ident
	*data32++ = 0; // loop type
				   // Note: This can be zero, but it won't loop.  Interesting.
	*data32++ = loopStart; // start
	*data32++ = loopEnd; // end
	*data32++ = 0; // fraction tuning
	*data32++ = 0; // num loops - ignored?

	memcpy(updated.Data() + initialDataStart + extraLoopInfoSize, at3.Data() + initialDataStart, at3.Size() - initialDataStart);
}

// Pass in 'full' to get the "set-once" parameters too (that don't change with every Decode call etc)
void LogAtracContext(int atracID, const u8 *buffer, const u8 *secondBuffer, bool full) {
	// Need to call this every time to get the values updated - but only in the emulator!
	// TODO: All Atrac calls should update the raw context, or we should just directly use the fields.
	int bufferOff = (intptr_t)buffer;
	int secondBufferOff = (intptr_t)secondBuffer;
	SceAtracId *ctx = _sceAtracGetContextAddress(atracID);
	if (!ctx) {
		schedf("Context not yet available for atracID %d\n", atracID);
		return;
	}
	if (full) {
		// Also log some stuff from the codec context, just because.
		// Actually, doesn't seem very useful. inBuf is just the current frame being decoded.
		// printf("sceAudioCodec inbuf: %p outbuf: %p inBytes: %d outBytes: %d\n", ctx->codec.inBuf, ctx->codec.outBuf, ctx->codec.inBytes, ctx->codec.outBytes);
		schedf("dataOff: %08x sampleSize: %04x codec: %04x channels: %d\n", ctx->info.dataOff, ctx->info.sampleSize, ctx->info.codec, ctx->info.numChan);
		schedf("endSample: %08x loopStart: %08x loopEnd: %08x\n", ctx->info.endSample, ctx->info.loopStart, ctx->info.loopEnd);
		schedf("bufferByte: %08x secondBufferByte: %08x\n", ctx->info.bufferByte, ctx->info.secondBufferByte);
	}

	schedf("decodePos: %08x curBuffer: %d state: %d framesToSkip: %d\n", ctx->info.decodePos, ctx->info.curBuffer, ctx->info.state, ctx->info.framesToSkip);
	schedf("curFileOff: %08x fileDataEnd: %08x loopNum: %d\n", ctx->info.curFileOff, ctx->info.fileDataEnd, ctx->info.loopNum);
	schedf("streamDataByte: %08x streamOff: %08x streamOff2: %08x codec_err: %03xX\n", ctx->info.streamDataByte, ctx->info.streamOff, ctx->info.secondStreamOff, ctx->codec.err >> 4); // we censor the last codec err digit, haven't figured it out.
	// Change the raw pointers to offsets to account for allocation differences when running under psplink.
	int bufOff = (ctx->info.buffer == 0) ? 0 : (intptr_t)ctx->info.buffer - bufferOff;
	int secondBufOff = (ctx->info.secondBuffer == 0) ? 0 : (intptr_t)ctx->info.secondBuffer - secondBufferOff;
	// Note, sometimes there's no secondbuffer use, but there's a value lingering in the register from a previous run.
	// In those cases, we replace it with a dummy value.
	if (!secondBuffer) {
		secondBufOff = 0xabcdabcd;
	}
	schedf("buffer(offset): %08x secondBuffer(offset): %08x firstValidSample: %04x\n", bufOff, secondBufOff, ctx->info.firstValidSample);
	//schedf("buffer %08x secondBuffer %08x firstValidSample: %04x\n", ctx->info.buffer, ctx->info.secondBuffer, ctx->info.firstValidSample);

	for (int i = 0; i < ARRAY_SIZE(ctx->info.unk); i++) {
		if (ctx->info.unk[i] != 0) {
			schedf("unk[%d]: %08x\n", i, ctx->info.unk[i]);
		}
	}
}

void LogResetBuffer(u32 result, int sample, const AtracResetBufferInfo &resetInfo, const u8 *bufPtr) {
	schedf("%08x=sceAtracGetBufferInfoForResetting(%d):\n", result, sample);
	for (int i = 0; i < 2; i++) {
		const AtracSingleResetBufferInfo &info = i == 0 ? resetInfo.first : resetInfo.second;
		schedf("  %s: writeOffset: %08x writableBytes: %08x minWriteBytes: %08x filePos: %08x\n",
			i == 0 ? "first " : "second", (info.writePos == (const u8*)0xcccccccc) ? 0xcccccccc : info.writePos - bufPtr, info.writableBytes, info.minWriteBytes, info.filePos);
	}
}

void LogResetBufferInfo(int atracID, const u8 *bufPtr) {
	static const int sampleOffsets[12] = { -2048, -10000, 0, 1, 2047, 2048, 2049, 4096, 16384, 20000, 40000, 1000000000, };
	for (size_t i = 0; i < ARRAY_SIZE(sampleOffsets); i++) {
		AtracResetBufferInfo resetInfo;
		memset(&resetInfo, 0xcc, sizeof(resetInfo));
		const u32 result = sceAtracGetBufferInfoForResetting(atracID, sampleOffsets[i], &resetInfo);
		LogResetBuffer(result, sampleOffsets[i], resetInfo, (const u8 *)bufPtr);
	}
}

const char *AtracTestModeToString(AtracTestMode mode) {
	switch (mode & ATRAC_TEST_MODE_MASK) {
	case ATRAC_TEST_FULL: return "full";
	case ATRAC_TEST_HALFWAY: return "halfway";
	case ATRAC_TEST_STREAM: return "stream";
	default: return "N/A";
	}
}
