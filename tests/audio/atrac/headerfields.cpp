#include "shared.h"

// What sceAtrac checks in the RIFF fact chunk: the end sample, and the sample offset after it.
//
// SetData fails with 0x80630008 when end sample + sample offset + the decoder delay (0x45 Atrac3,
// 0x170 Atrac3+) is more than the samples the data chunk's whole frames hold. An offset of 0 counts
// as a whole frame there. The RIFF size and a data chunk longer than the file don't matter.

static void put32(u8 *p, u32 v) {
	p[0] = v & 0xFF;
	p[1] = (v >> 8) & 0xFF;
	p[2] = (v >> 16) & 0xFF;
	p[3] = (v >> 24) & 0xFF;
}

struct Layout {
	int endAt;
	int offsetAt;
	int dataSizeAt;
	int frameBytes;
	int samplesPerFrame;
	int delay;
};

static u8 *withFact(Atrac3File &at3, const Layout &l, u32 dataBytes, u32 offset, u32 end) {
	u8 *data = new u8[at3.Size()];
	memcpy(data, at3.Data(), at3.Size());
	put32(data + l.dataSizeAt, dataBytes);
	put32(data + l.offsetAt, offset);
	put32(data + l.endAt, end);
	return data;
}

static void tryLimit(const char *title, Atrac3File &at3, const Layout &l, u32 dataBytes, u32 offset) {
	const u32 total = dataBytes / l.frameBytes * l.samplesPerFrame;
	const u32 limit = total - l.delay - (offset ? offset : l.samplesPerFrame);
	for (int extra = 0; extra < 2; ++extra) {
		u8 *data = withFact(at3, l, dataBytes, offset, limit + extra);
		int id = sceAtracSetDataAndGetID(data, at3.Size());
		if (id >= 0) {
			sceAtracReleaseAtracID(id);
		}
		int hid = sceAtracSetHalfwayBufferAndGetID(data, at3.Size(), at3.Size());
		if (hid >= 0) {
			sceAtracReleaseAtracID(hid);
		}
		checkpoint("  %s, offset %x, end at the limit%s: SetData %08x, Halfway %08x", title, offset, extra ? " + 1" : "", id < 0 ? id : 0, hid < 0 ? hid : 0);
		delete [] data;
	}
}

static void describe(const char *title, Atrac3File &at3, const Layout &l, u32 dataBytes, u32 offset, u32 end) {
	u8 *data = withFact(at3, l, dataBytes, offset, end);
	int id = sceAtracSetDataAndGetID(data, at3.Size());
	if (id < 0) {
		checkpoint("  %s: SetData %08x", title, id);
		delete [] data;
		return;
	}
	int endSample = -1, loopStart = -1, loopEnd = -1;
	sceAtracGetSoundSample(id, &endSample, &loopStart, &loopEnd);
	u32 pos = 0xFFFFFFFF;
	sceAtracGetNextDecodePosition(id, &pos);
	s16 *buf = new s16[2048 * 2];
	int count = -1, ended = 0, remaining = 0;
	int result = sceAtracDecodeData(id, (u16 *)buf, &count, &ended, &remaining);
	checkpoint("  %s: end sample %d, next position %d, first decode %08x with %d samples", title, endSample, pos, result, count);
	delete [] buf;
	sceAtracReleaseAtracID(id);
	delete [] data;
}

extern "C" int main(int argc, char *argv[]) {
	// From gen_atrac3.py: 70 0xC0-byte frames behind LocoRoco 2's header, which declares 200000.
	Atrac3File at3("atrac3_c0_mono.at3");
	at3.Require();
	Atrac3File plus("sample.at3");
	plus.Require();
	LoadAtrac();

	const Layout at3Layout = { 0x3C, 0x40, 0x48, 0xC0, 1024, 0x45 };
	const Layout plusLayout = { 0x50, 0x54, 0x5C, 376, 2048, 0x170 };
	const u32 at3Data = (at3.Size() - 0x4C) / 0xC0 * 0xC0;
	const u32 plusData = plus.Size() - 0x60;

	checkpointNext("End sample limit:");
	static const u32 at3Offsets[] = { 0, 1, 0x200, 0x400, 0x500, 0x1000 };
	for (u32 offset : at3Offsets) {
		tryLimit("Atrac3", at3, at3Layout, at3Data, offset);
	}
	static const u32 plusOffsets[] = { 0, 1, 0x400, 0x800, 0x900, 0x1000 };
	for (u32 offset : plusOffsets) {
		tryLimit("Atrac3+", plus, plusLayout, plusData, offset);
	}

	checkpointNext("Other fields:");
	{
		u8 *data = new u8[at3.Size()];
		memcpy(data, at3.Data(), at3.Size());
		put32(data + 0x04, 1);
		int id = sceAtracSetDataAndGetID(data, at3.Size());
		checkpoint("  RIFF size 1: %08x", id < 0 ? id : 0);
		if (id >= 0) {
			sceAtracReleaseAtracID(id);
		}
		delete [] data;
	}

	checkpointNext("Offset 0 against a whole frame:");
	describe("Atrac3, offset 0", at3, at3Layout, at3Data, 0, 60000);
	describe("Atrac3, offset 0x400", at3, at3Layout, at3Data, 0x400, 60000);
	describe("Atrac3, offset 0x200", at3, at3Layout, at3Data, 0x200, 60000);
	describe("Atrac3+, offset 0", plus, plusLayout, plusData, 0, 200000);
	describe("Atrac3+, offset 0x800", plus, plusLayout, plusData, 0x800, 200000);
	describe("Atrac3+, offset 0x400", plus, plusLayout, plusData, 0x400, 200000);

	UnloadAtrac();
	return 0;
}
