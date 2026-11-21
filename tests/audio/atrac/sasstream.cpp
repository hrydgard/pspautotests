// Streams a mono Atrac3 file through a SAS voice the way L.A. Gridlock does: SetDataAndGetID on the
// first chunk, then __sceSasConcatenateATRAC3 for each following chunk. Some cases zero out the start
// of a chunk (the game does this to its last, short chunk), to see what the voice does with frames
// that don't decode. Samples aren't compared, only whether a grain is silent.

#include <common.h>
#include <psputility.h>
#include <pspatrac3.h>

#include "../sascore/sascore.h"
#include "atrac.h"
#include "shared.h"

__attribute__((aligned(64))) SasCore sasCore;
__attribute__((aligned(64))) short samples[256 * 2];

static const int GRAIN = 256;
static const int VOICE = 0;
static const int CHUNK = 0x1000;
static const int REPEATS = 4;
static const int FRAMES_PER_COPY = 67;
static const int MAX_GRAINS = 1500;

static u8 *fileData;
static int fileSize;
static const int MAX_CHUNKS = 16;
// One buffer per chunk, so nothing is overwritten however many chunks the voice accepts.
static __attribute__((aligned(64))) u8 buffers[MAX_CHUNKS][CHUNK];

// test_mono.at3 with its 67 frames repeated, so it takes several chunks.
static void BuildFile(Atrac3File &at3) {
	const int dataOff = 80;
	const int dataSize = at3.Size() - dataOff;
	fileSize = dataOff + dataSize * REPEATS;
	fileData = new u8[fileSize];
	memcpy(fileData, at3.Data(), dataOff);
	for (int i = 0; i < REPEATS; ++i) {
		memcpy(fileData + dataOff + dataSize * i, at3.Data() + dataOff, dataSize);
	}
	u32 *riffSize = (u32 *)(fileData + 4);
	*riffSize = fileSize - 8;
	// fact: total samples, minus the same trailing padding as the original.
	u32 *factSamples = (u32 *)(fileData + 60);
	*factSamples = FRAMES_PER_COPY * 1024 * REPEATS - (FRAMES_PER_COPY * 1024 - *factSamples);
	u32 *dataSizePtr = (u32 *)(fileData + 76);
	*dataSizePtr = dataSize * REPEATS;
}

static const char *BufName(const u8 *p) {
	static char temp[32];
	if (p == (const u8 *)0xFFFFFFFF) {
		return "FFFFFFFF";
	}
	if (p == NULL) {
		return "0";
	}
	for (int i = 0; i < MAX_CHUNKS; ++i) {
		if (p >= buffers[i] && p < buffers[i] + CHUNK) {
			snprintf(temp, sizeof(temp), "chunk%d+%x", i, (int)(p - buffers[i]));
			return temp;
		}
	}
	return "other";
}

struct GrainState {
	int coreResult;
	int concatResult;
	int ended;
	int heightBucket;
	int sound;
	char secondBuffer[32];
	u32 secondBufferByte;

	bool operator ==(const GrainState &o) const {
		return coreResult == o.coreResult && concatResult == o.concatResult && ended == o.ended && heightBucket == o.heightBucket && sound == o.sound && !strcmp(secondBuffer, o.secondBuffer) && secondBufferByte == o.secondBufferByte;
	}
};

static void PrintState(int first, int last, const GrainState &s) {
	schedf("  grains %4d-%4d: core=%08x concat=%08x end=%d height=%d %s second=%s/%x\n", first, last, s.coreResult, s.concatResult, s.ended, s.heightBucket, s.sound ? "sound " : "silent", s.secondBuffer, s.secondBufferByte);
}

// zeroChunk: which concatenated chunk (1 = the first after the SetData one, -1 = the last) gets its
// first zeroBytes cleared. signalEnd: after the last chunk, concatenate (0, 0) like the game does.
static void RunCase(const char *title, int zeroChunk, int zeroBytes, bool signalEnd) {
	schedf("%s:\n", title);

	const int numChunks = (fileSize + CHUNK - 1) / CHUNK;
	memset(buffers, 0, sizeof(buffers));
	memcpy(buffers[0], fileData, CHUNK);

	int atracID = sceAtracSetDataAndGetID(buffers[0], CHUNK);
	if (atracID < 0) {
		schedf("  sceAtracSetDataAndGetID: %08x\n", atracID);
		return;
	}
	SceAtracId *ctx = _sceAtracGetContextAddress(atracID);
	schedf("  set: state=%d streamOff=%x streamDataByte=%x fileDataEnd=%x\n", ctx->info.state, ctx->info.streamOff, ctx->info.streamDataByte, ctx->info.fileDataEnd);

	// The same adjustments as sas.cpp, which are also what L.A. Gridlock does.
	ctx->info.state = 0x10;
	int result = __sceSasSetVoiceATRAC3(&sasCore, VOICE, ctx);
	schedf("  __sceSasSetVoiceATRAC3: %08x\n", result);
	ctx->info.buffer += ctx->info.streamOff;
	ctx->info.loopNum = VOICE;
	ctx->info.curFileOff += ctx->info.streamDataByte;
	ctx->info.secondBuffer = (u8 *)0xFFFFFFFF;

	result = __sceSasSetADSR(&sasCore, VOICE, 15, 0x40000000, 0, 0, 0x10000);
	schedf("  __sceSasSetADSR: %08x\n", result);
	result = __sceSasSetKeyOn(&sasCore, VOICE);
	schedf("  __sceSasSetKeyOn: %08x\n", result);

	int nextChunk = 1;
	bool endSent = false;
	GrainState prev;
	int runStart = 0;
	int endedGrains = 0;
	int g;
	for (g = 0; g < MAX_GRAINS; ++g) {
		GrainState s;
		memset(&s, 0, sizeof(s));
		s.concatResult = -1;

		if (nextChunk < numChunks) {
			u8 *buf = buffers[nextChunk];
			const int offset = nextChunk * CHUNK;
			const int size = offset + CHUNK > fileSize ? fileSize - offset : CHUNK;
			memcpy(buf, fileData + offset, size);
			const bool isLast = nextChunk == numChunks - 1;
			if (nextChunk == zeroChunk || (zeroChunk == -1 && isLast)) {
				memset(buf, 0, zeroBytes);
			}
			s.concatResult = __sceSasConcatenateATRAC3(&sasCore, VOICE, buf, size);
			if (s.concatResult == 0) {
				schedf("  grain %d: concatenated chunk %d (%x bytes)%s\n", g, nextChunk, size, isLast ? ", last" : "");
				nextChunk++;
			}
		} else if (signalEnd && !endSent) {
			s.concatResult = __sceSasConcatenateATRAC3(&sasCore, VOICE, NULL, 0);
			if (s.concatResult == 0) {
				schedf("  grain %d: concatenated the end (0, 0)\n", g);
				endSent = true;
			}
		}

		memset(samples, 0, sizeof(samples));
		s.coreResult = __sceSasCore(&sasCore, samples);
		s.ended = (__sceSasGetEndFlag(&sasCore) >> VOICE) & 1;
		int height = __sceSasGetEnvelopeHeight(&sasCore, VOICE);
		s.heightBucket = height == 0 ? 0 : (height == PSP_SAS_ENVELOPE_HEIGHT_MAX ? 2 : 1);
		for (int i = 0; i < GRAIN * 2; ++i) {
			if (samples[i] != 0) {
				s.sound = 1;
				break;
			}
		}
		snprintf(s.secondBuffer, sizeof(s.secondBuffer), "%s", BufName(ctx->info.secondBuffer));
		s.secondBufferByte = ctx->info.secondBufferByte;

		if (g == 0) {
			prev = s;
		} else if (!(s == prev)) {
			PrintState(runStart, g - 1, prev);
			prev = s;
			runStart = g;
		}

		if (s.ended) {
			if (++endedGrains >= 16) {
				break;
			}
		} else {
			endedGrains = 0;
		}
	}
	PrintState(runStart, g >= MAX_GRAINS ? MAX_GRAINS - 1 : g, prev);
	schedf("  stopped after %d grains, %d chunks concatenated of %d\n", g >= MAX_GRAINS ? MAX_GRAINS : g + 1, nextChunk - 1, numChunks - 1);
	schedf("  ctx: state=%d curFileOff=%x decodePos=%x buffer=%s\n", ctx->info.state, ctx->info.curFileOff, ctx->info.decodePos, BufName(ctx->info.buffer));

	result = __sceSasSetKeyOff(&sasCore, VOICE);
	// Let the release finish.
	for (int i = 0; i < 4; ++i) {
		__sceSasCore(&sasCore, samples);
	}
	result = __sceSasUnsetATRAC3(&sasCore, VOICE);
	schedf("  __sceSasUnsetATRAC3: %08x\n", result);
	result = sceAtracReleaseAtracID(atracID);
	schedf("  sceAtracReleaseAtracID while state is 0x10: %08x\n", result);
	// The game sets the state to 1 before releasing.
	ctx->info.state = 1;
	result = sceAtracReleaseAtracID(atracID);
	schedf("  sceAtracReleaseAtracID: %08x\n", result);
}

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_SASCORE);
	sceUtilityLoadModule(PSP_MODULE_AV_ATRAC3PLUS);

	Atrac3File at3("test_mono.at3");
	at3.Require();
	BuildFile(at3);
	schedf("file: %x bytes, %d chunks\n", fileSize, (fileSize + CHUNK - 1) / CHUNK);

	int result = __sceSasInit(&sasCore, GRAIN, 32, 0, 44100);
	schedf("__sceSasInit: %08x\n", result);

	RunCase("Clean, no end signalled", 0, 0, false);
	RunCase("Clean", 0, 0, true);
	RunCase("Zeroed 0x400 at the start of chunk 3", 3, 0x400, true);
	RunCase("Zeroed 0x400 at the start of the last chunk", -1, 0x400, true);
	RunCase("Zeroed 0x98 (one frame) at the start of chunk 3", 3, 0x98, true);

	delete [] fileData;
	return 0;
}
