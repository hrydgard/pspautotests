#include "shared.h"
#include <malloc.h>
#include <pspthreadman.h>

// Big enough for the largest count tried with a valid rate, in case a mic is present and fills it.
#define BUF_SAMPLES 0x200000
static short *buf;

// A call that's accepted starts a capture, which keeps later calls busy, so wait for it.
// Sleep for its length first, so the reschedule doesn't depend on how fast the capture is.
static void waitIdle(int samples, int freq) {
	int i;
	sceKernelDelayThread((int)((long long)samples * 1000000 / freq) + 20000);
	for (i = 0; i < 200; ++i) {
		if (sceAudioPollInputEnd() == 0) {
			return;
		}
		sceKernelDelayThread(10000);
	}
	checkpoint("    (still busy)");
}

static void testInputBuf(const char *title, int samples, int freq, void *p) {
	int result = sceAudioInput(samples, freq, p);
	// Whether an accepted call reschedules varies (the first after init doesn't), so no marker.
	schedf("  %s: %08x\n", title, result);
	if (result >= 0) {
		waitIdle(samples, freq);
	}
}

static void testInput(const char *title, int samples, int freq) {
	testInputBuf(title, samples, freq, buf);
}

static void runTests() {
	char title[64];
	int i;

	testInput("Rate 0", 64, 0);
	testInputBuf("Rate 0, NULL buf", 64, 0, NULL);
	testInputBuf("NULL buf", 64, 44100, NULL);

	const static int freqs[] = {-1, 0, 1, 8000, 11025, 12000, 16000, 22050, 24000, 32000, 48000, 44100};
	for (i = 0; i < ARRAY_SIZE(freqs); ++i) {
		snprintf(title, sizeof(title), "Rate %d", freqs[i]);
		testInput(title, 64, freqs[i]);
	}

	// The size is checked before the rate, so rate 0 shows which sizes are accepted without capturing.
	const static int counts[] = {-64, -1, 0, 1, 63, 64, 65, 128, 160, 0xFFC0, 0x10000, 0x100000, 0x100001, 0x1000000, 0x7FFFFFC0};
	for (i = 0; i < ARRAY_SIZE(counts); ++i) {
		snprintf(title, sizeof(title), "%d samples at 0 Hz", counts[i]);
		testInput(title, counts[i], 0);
	}

	const static int validCounts[] = {64, 0xFFC0, 0x10000};
	for (i = 0; i < ARRAY_SIZE(validCounts); ++i) {
		snprintf(title, sizeof(title), "%d samples at 44100 Hz", validCounts[i]);
		testInput(title, validCounts[i], 44100);
	}
}

int main(int argc, char *argv[]) {
	buf = (short *)memalign(64, BUF_SAMPLES * sizeof(short));

	checkpointNext("Before init:");
	runTests();

	checkpointNext("sceAudioInputInit:");
	checkpoint("  Init: %08x", sceAudioInputInit(0, 0, 0));
	runTests();

	checkpointNext("sceAudioInputBlocking:");
	checkpoint("  Rate 0: %08x", sceAudioInputBlocking(64, 0, buf));
	checkpoint("  0 samples: %08x", sceAudioInputBlocking(0, 0, buf));
	checkpoint("  64 samples at 44100 Hz: %08x", sceAudioInputBlocking(64, 44100, buf));

	free(buf);
	return 0;
}
