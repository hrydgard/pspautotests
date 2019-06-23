#include <common.h>
#include <pspaudio.h>
#include <pspthreadman.h>

static void test2Reserve(const char *title, int samples) {
	int result = sceAudioOutput2Reserve(samples);
	if (result >= 0) {
		sceAudioOutput2Release();
	}
	checkpoint("%s: %08x", title, result);
}

static void testSRCReserve(const char *title, int samples, int freq, int fmt) {
	int result = sceAudioSRCChReserve(samples, freq, fmt);
	if (result >= 0) {
		sceAudioSRCChRelease();
	}
	checkpoint("%s: %08x", title, result);
}

extern "C" int main(int argc, char *argv[]) {
	checkpointNext("Output2");
	sceAudioOutput2Reserve(17);
	checkpoint("  Twice: %08x", sceAudioOutput2Reserve(17));
	sceAudioOutput2Release();

	test2Reserve("  Zero", 0);
	test2Reserve("  Negative", -1);
	static const int sampleCounts[] = { 16, 17, 18, 2048, 2049, 4110, 4111, 4112, 8222, 0x80000000 | 4111, 0x40000000 | 4111, 0x80000000 | 16, 0x80000000 | 17 };
	for (size_t i = 0; i < ARRAY_SIZE(sampleCounts); ++i) {
		char temp[256];
		sprintf(temp, "  %d samples", sampleCounts[i]);
		test2Reserve(temp, sampleCounts[i]);
	}

	checkpointNext("SRC");
	sceAudioSRCChReserve(17, 44100, 2);
	checkpoint("  SRC Twice: %08x", sceAudioSRCChReserve(17, 44100, 2));
	sceAudioSRCChRelease();

	checkpointNext("SRC samples");
	testSRCReserve("  Zero SRC samples", 0, 44100, 2);
	testSRCReserve("  Negative SRC samples", -1, 44100, 2);
	for (size_t i = 0; i < ARRAY_SIZE(sampleCounts); ++i) {
		char temp[256];
		sprintf(temp, "  %d SRC samples", sampleCounts[i]);
		testSRCReserve(temp, sampleCounts[i], 44100, 2);
	}

	checkpointNext("SRC frequencies");
	testSRCReserve("  Zero frequency", 64, 0, 2);
	testSRCReserve("  Negative frequency", 64, -1, 2);
	static const int freqs[] = { 1, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 4000, 40000, 88200, 64000, 360000, 0x80000000 | 44100 };
	for (size_t i = 0; i < ARRAY_SIZE(freqs); ++i) {
		char temp[256];
		sprintf(temp, "  %d frequency", freqs[i]);
		testSRCReserve(temp, 64, freqs[i], 2);
	}

	checkpointNext("SRC formats");
	testSRCReserve("  Zero format", 64, 0, 0);
	testSRCReserve("  Negative format", 64, 0, -2);
	static const int formats[] = { 1, 2, 3, 4, 5, 8, 0x80000002 };
	for (size_t i = 0; i < ARRAY_SIZE(formats); ++i) {
		char temp[256];
		sprintf(temp, "  %d format", formats[i]);
		testSRCReserve(temp, 64, 0, formats[i]);
	}

	return 0;
}