#include <common.h>
#include <pspaudio.h>
#include <pspthreadman.h>

static u16 data[8192];

static void testSRCFrequency(const char *title, int samples, int freq) {
	checkpoint("  %s Reserve: %08x", title, sceAudioSRCChReserve(samples, freq, 2));
	checkpoint("  %s OutputBlocking: %08x", title, sceAudioSRCOutputBlocking(0x7fff, data));
	u64 st = sceKernelGetSystemTimeWide();
	while (sceAudioOutput2GetRestSample() != 0) {
		u64 n = sceKernelGetSystemTimeWide();
		if (n > st + 1000000) {
			break;
		}
	}
	u64 et = sceKernelGetSystemTimeWide();
	int realFreq = freq == 0 ? 44100 : freq;
	checkpoint("  %s: %dXXX / %d", title, (int)(et - st) / 1000, samples * 1000000 / realFreq);
	checkpoint("  %s Release: %08x", title, sceAudioSRCChRelease());
}

static void test2Frequency(const char *title, int samples, int freq) {
	checkpoint("  %s Reserve: %08x %08x", title, sceAudioChReserve(0, samples + 64, 0), sceAudioSRCChReserve(samples, freq, 2));
	checkpoint("  %s OutputBlocking: %08x %08x", title, sceAudioOutputBlocking(0, 0x7fff, data), sceAudioSRCOutputBlocking(0x7fff, data));
	u64 st = sceKernelGetSystemTimeWide();
	u64 et0 = 0, et2 = 0;
	while (true) {
		u64 n = sceKernelGetSystemTimeWide();

		if (sceAudioGetChannelRestLen(0) == 0 && et0 == 0) {
			et0 = n;
		}
		if (sceAudioOutput2GetRestSample() == 0 && et2 == 0) {
			et2 = n;
		}

		if (n > st + 1000000 || (et0 != 0 && et2 != 0)) {
			break;
		}
	}
	int realFreq = freq == 0 ? 44100 : freq;
	checkpoint("  %s: src=%dXXX, ch=%dXXX / f=%d, 44.1=%d", title, (int)(et2 - st) / 1000, (int)(et0 - st) / 1000, samples * 1000000 / realFreq, samples * 1000000 / 44100);
	checkpoint("  %s Release: %08x %08x", title, sceAudioSRCChRelease(), sceAudioChRelease(0));
}

extern "C" int main(int argc, char *argv[]) {
	checkpointNext("SRC");
	testSRCFrequency("22.05kHz", 2048, 22050);
	testSRCFrequency("36kHz", 2048, 36000);
	testSRCFrequency("44.1kHz", 2048, 44100);
	testSRCFrequency("48kHz", 2048, 48000);
	testSRCFrequency("Zero", 2048, 0);

	checkpointNext("Multi-channel");
	test2Frequency("48kHz", 2048, 48000);

	return 0;
}