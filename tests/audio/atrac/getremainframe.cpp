#include "shared.h"

void testGetRemainFrame(const char *title, int atracID, bool withRemainFrame) {
	int remainFrame = -1337;
	int result = sceAtracGetRemainFrame(atracID, withRemainFrame ? &remainFrame : NULL);
	if (result >= 0) {
		checkpoint("%s: OK (%08x), remainFrame=%d", title, result, remainFrame);
	} else if (atracID < 0) {
		checkpoint("%s: Invalid (%08x, %08x), remainFrame=%d", title, atracID, result, remainFrame);
	} else {
		checkpoint("%s: Failed (%08x), remainFrame=%d", title, result, remainFrame);
	}
}

void drainStreamedAtrac(int atracID, Atrac3File &at3) {
	int samples, end, remain;
	u32 next;
	do {
		int res = sceAtracDecodeData(atracID, NULL, &samples, &end, &remain);
		if (res != 0) {
			checkpoint("** ERROR: %08x", res);
			break;
		}

		if (remain == 0) {
			u8 *writePtr;
			u32 avail, off;
			if (sceAtracGetStreamDataInfo(atracID, &writePtr, &avail, &off) == 0) {
				memcpy(writePtr, at3.Data() + off, avail);
				sceAtracAddStreamData(atracID, avail);
			}
		}
	} while (end == 0);
}

void testAtracIDs(Atrac3File &at3) {
	int atracID = sceAtracSetDataAndGetID(at3.Data(), at3.Size());

	checkpointNext("Atrac IDs:");
	testGetRemainFrame("  Normal", atracID, true);
	testGetRemainFrame("  Invalid ID", -1, true);
	testGetRemainFrame("  Unused ID", 2, true);
	sceAtracReleaseAtracID(atracID);
}

void testAtracMisc(Atrac3File &at3) {
	int atracID = sceAtracSetDataAndGetID(at3.Data(), at3.Size());
	int atracID2 = sceAtracGetAtracID(PSP_ATRAC_AT3PLUS);

	checkpointNext("Other states:");
	testGetRemainFrame("  No data", atracID2, true);
	forceAtracState(atracID, 8);
	testGetRemainFrame("  State 8", atracID, true);
	forceAtracState(atracID, 16);
	testGetRemainFrame("  State 16", atracID, true);
	forceAtracState(atracID, 2);
	// Crashes.
	//testGetRemainFrame("  No out param", atracID, false);
	sceAtracReleaseAtracID(atracID);
	sceAtracReleaseAtracID(atracID2);
}

void testAtracBuffers(Atrac3File &at3) {
	u8 *input = new u8[at3.Size()];
	int atracID;

	memcpy(input, at3.Data(), at3.Size());
	checkpointNext("Buffers:");
	atracID = sceAtracSetDataAndGetID(input, at3.Size());
	testGetRemainFrame("  Full buffer", atracID, true);
	sceAtracReleaseAtracID(atracID);

	static const int halfSizes[] = {472, 473, 847, 848, 1975, 1976};
	for (size_t i = 0; i < ARRAY_SIZE(halfSizes); ++i) {
		char temp[128];
		snprintf(temp, 128, "  Half buffer size = %d", halfSizes[i]);
		atracID = sceAtracSetHalfwayBufferAndGetID(input, halfSizes[i], at3.Size());
		testGetRemainFrame(temp, atracID, true);
		sceAtracReleaseAtracID(atracID);
	}

	atracID = sceAtracSetHalfwayBufferAndGetID(input, at3.Size() / 2, at3.Size() / 2);
	testGetRemainFrame("  Streaming buffer", atracID, true);
	sceAtracReleaseAtracID(atracID);

	memcpy(input, at3.Data(), at3.Size());
	atracID = sceAtracSetHalfwayBufferAndGetID(input, at3.Size() / 2, at3.Size() / 2);
	AtracResetBufferInfo info = {};
	sceAtracGetBufferInfoForResetting(atracID, 4096, &info);
	memcpy(input, at3.Data() + info.first.filePos, info.first.minWriteBytes);
	sceAtracResetPlayPosition(atracID, 4096, info.first.minWriteBytes, 0);
	testGetRemainFrame("  Streaming buffer after seek + min", atracID, true);
	sceAtracReleaseAtracID(atracID);

	delete [] input;
}

void createLoopedAtracFrom(Atrac3File &at3, Atrac3File &updated) {
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
	*data32++ = 1; // num loops
	*data32++ = 0x18; // extra smpl bytes at end (seems incorrect, but found in data.)
	// Loop info itself.
	*data32++ = 0; // ident
	*data32++ = 0; // loop type
	// Note: This can be zero, but it won't loop.  Interesting.
	*data32++ = 2048; // start
	*data32++ = 249548; // end
	*data32++ = 0; // fraction tuning
	*data32++ = 0; // num loops - ignored?

	memcpy(updated.Data() + initialDataStart + extraLoopInfoSize, at3.Data() + initialDataStart, at3.Size() - initialDataStart);
}

void testAtracDrained(Atrac3File &at3) {
	Atrac3File looped((size_t)0);
	createLoopedAtracFrom(at3, looped);
	u8 *input = new u8[looped.Size()];
	int atracID;

	checkpointNext("Drained remain values:");

	memcpy(input, at3.Data(), at3.Size());
	atracID = sceAtracSetDataAndGetID(input, at3.Size());
	drainStreamedAtrac(atracID, at3);
	testGetRemainFrame("  Full buffer", atracID, true);
	sceAtracReleaseAtracID(atracID);

	memcpy(input, at3.Data(), at3.Size());
	atracID = sceAtracSetHalfwayBufferAndGetID(input, at3.Size() / 2, at3.Size());
	drainStreamedAtrac(atracID, at3);
	testGetRemainFrame("  Half buffer", atracID, true);
	sceAtracReleaseAtracID(atracID);

	memcpy(input, at3.Data(), at3.Size());
	atracID = sceAtracSetHalfwayBufferAndGetID(input, at3.Size() / 2, at3.Size() / 2);
	drainStreamedAtrac(atracID, at3);
	testGetRemainFrame("  Streamed without loop", atracID, true);
	sceAtracReleaseAtracID(atracID);

	memcpy(input, looped.Data(), looped.Size());
	atracID = sceAtracSetHalfwayBufferAndGetID(input, at3.Size() / 2, at3.Size() / 2);
	sceAtracSetLoopNum(atracID, 1);
	drainStreamedAtrac(atracID, looped);
	testGetRemainFrame("  Streamed with loop", atracID, true);
	sceAtracReleaseAtracID(atracID);

	delete[] input;
}

extern "C" int main(int argc, char *argv[]) {
	Atrac3File at3("sample.at3");
	at3.Require();
	LoadAtrac();

	testAtracIDs(at3);
	testAtracMisc(at3);
	testAtracBuffers(at3);
	testAtracDrained(at3);

	UnloadAtrac();
	return 0;
}