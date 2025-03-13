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
		printf("TEST FAILURE: unable to read sample.at3\n");
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
void LogAtracContext(int atracID, u32 buffer, bool full) {
	// Need to call this every time to get the values updated - but only in the emulator!
	// TODO: All Atrac calls should update the raw context, or we should just directly use the fields.
	SceAtracId *ctx = _sceAtracGetContextAddress(atracID);
	if (!ctx) {
		schedf("Context not yet available for atracID %d\n", atracID);
		return;
	}
	if (full) {
		// Also log some stuff from the codec context, just because.
		// Actually, doesn't seem very useful. inBuf is just the current frame being decoded.
		/*
		printf("CODEC inbuf: %p\n", ctx->codec.inBuf);
		printf("CODEC outbuf: %p\n", ctx->codec.outBuf);
		printf("CODEC edramAddr: %08x\n", ctx->codec.edramAddr);
		// printf("CODEC allocMem: %p\n", ctx->codec.allocMem);
		printf("CODEC neededMem: %d\n", ctx->codec.neededMem);
		printf("CODEC err: %d\n", ctx->codec.err);
		*/
		schedf("dataOff: %08x sampleSize: %04x codec: %04x channels: %d\n", ctx->info.dataOff, ctx->info.sampleSize, ctx->info.codec, ctx->info.numChan);
		schedf("endSample: %08x loopStart: %08x loopEnd: %08x\n", ctx->info.endSample, ctx->info.loopStart, ctx->info.loopEnd);
		schedf("bufferByte: %08x secondBufferByte: %08x\n", ctx->info.bufferByte, ctx->info.secondBufferByte);
	}

	schedf("decodePos: %08x unk22: %08x state: %d numFrame: %02x\n", ctx->info.decodePos, ctx->info.unk22, ctx->info.state, ctx->info.numFrame);
	schedf("curOff: %08x dataEnd: %08x loopNum: %d\n", ctx->info.curOff, ctx->info.dataEnd, ctx->info.loopNum);
	schedf("streamDataByte: %08x streamOff: %08x unk52: %08x codec_err: %08x\n", ctx->info.streamDataByte, ctx->info.streamOff, ctx->info.unk52, ctx->codec.err);
	// Probably shouldn't log these raw pointers (buffer/secondBuffer).
	schedf("buffer(offset): %08x secondBuffer: %08x samplesPerChan: %04x\n", ctx->info.buffer - buffer, ctx->info.secondBuffer, ctx->info.samplesPerChan);

	for (int i = 0; i < ARRAY_SIZE(ctx->info.unk); i++) {
		if (ctx->info.unk[i] != 0) {
			schedf("unk[%d]: %08x\n", i, ctx->info.unk[i]);
		}
	}
}
