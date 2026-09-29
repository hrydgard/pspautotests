#include "shared.h"

// A 2-channel, normal-stereo Atrac3 header over 0xC0-byte frames that each hold one mono sound unit
// (atrac3_c0_mono.at3, from gen_atrac3.py). LocoRoco 2 streams its MuiMui house music like this: it
// writes the same header for every track. libatrac3plus picks the codec parameter by frame size and
// joint stereo alone, which for 0xC0 without joint stereo is the mono one. The same frames under a
// 1-channel header, and a joint-stereo one, for comparison. Samples aren't compared: our decoder
// isn't bit exact with the ME's.

enum Variant {
	AS_SHIPPED,
	MONO_HEADER,
	JOINT_HEADER,
};

static u8 *MakeVariant(Atrac3File &at3, Variant v) {
	u8 *data = new u8[at3.Size()];
	memcpy(data, at3.Data(), at3.Size());
	if (v == MONO_HEADER) {
		data[0x16] = 1;
	} else if (v == JOINT_HEADER) {
		// The coding mode, and its copy that must match.
		data[0x2C] = 1;
		data[0x2E] = 1;
	}
	return data;
}

static void DecodeAll(const char *title, Atrac3File &at3, Variant v) {
	u8 *data = MakeVariant(at3, v);
	// The header is the game's own, sized for streaming, so feed it the way the game does.
	int atracID = sceAtracSetHalfwayBufferAndGetID(data, at3.Size(), at3.Size());
	if (atracID < 0) {
		checkpoint("%s: set failed: %08x", title, atracID);
		delete [] data;
		return;
	}

	u32 channels = 0x1337;
	int result = sceAtracGetChannel(atracID, &channels);
	checkpoint("%s: channels=%08x (%d)", title, result, channels);

	s16 *buf = new s16[1024 * 2];
	int total = 0;
	int calls = 0;
	int frameResult = 0;
	bool audible = false;
	bool leftIsRight = true;
	for (calls = 0; calls < 200; ++calls) {
		int count = 0, ended = 0, remaining = 0;
		memset(buf, 0, 1024 * 2 * sizeof(s16));
		frameResult = sceAtracDecodeData(atracID, (u16 *)buf, &count, &ended, &remaining);
		if (frameResult != 0) {
			break;
		}
		for (int i = 0; i < count; ++i) {
			if (buf[i * channels] != 0) {
				audible = true;
			}
			if (channels == 2 && buf[i * 2] != buf[i * 2 + 1]) {
				leftIsRight = false;
			}
		}
		total += count;
		if (ended) {
			break;
		}
	}
	checkpoint("%s: %d calls, %d samples, last result %08x, audible=%d, left==right=%d", title, calls, total, frameResult, audible, leftIsRight);

	delete [] buf;
	sceAtracReleaseAtracID(atracID);
	delete [] data;
}

extern "C" int main(int argc, char *argv[]) {
	Atrac3File at3("atrac3_c0_mono.at3");
	at3.Require();
	LoadAtrac();

	checkpointNext("Decoding:");
	DecodeAll("  2-channel header", at3, AS_SHIPPED);
	DecodeAll("  Mono header", at3, MONO_HEADER);
	DecodeAll("  Joint stereo header", at3, JOINT_HEADER);

	UnloadAtrac();
	return 0;
}
