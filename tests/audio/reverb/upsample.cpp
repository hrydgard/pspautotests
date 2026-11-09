#include <common.h>
#include <pspkernel.h>
#include <psputility.h>
#include "../sascore/sascore.h"

// How the SAS reverb moves between 44.1khz and its internal 22.05khz. With dry off, the output is
// the wet signal alone, so the parity of its zero frames shows how it is upsampled, and feeding a
// 22.05khz tone (+x, -x, ...) shows how the send is downsampled: dropping one frame of each pair
// turns that tone into DC, averaging the pair cancels it.
//
// Only structural facts are printed. The reverb keeps its state between cases (and runs), so its
// exact levels aren't reproducible.

static const int GRAIN = 256;
static const int SETTLE_GRAINS = 40;
static const int CAPTURE_GRAINS = 8;
static const int FRAMES = GRAIN * CAPTURE_GRAINS;

__attribute__((aligned(64))) SasCore sasCore;
__attribute__((aligned(64))) short pcm[256];
// Twice the size needed: __sceSasCore's cache handling covers more than one stereo grain (in
// multichannel mode it is sized for four channels), and with a variable right after the buffer that
// discarded its pending write - stdout's write hook, here, which crashed.
__attribute__((aligned(64))) short samples[FRAMES * 2 * 2];
__attribute__((aligned(64))) short scratch[GRAIN * 2 * 2];

static void fillPCM(bool alternating) {
	for (int i = 0; i < 256; ++i) {
		pcm[i] = alternating && (i & 1) ? -16384 : 16384;
	}
	sceKernelDcacheWritebackRange(pcm, sizeof(pcm));
}

static const char *describeLevel(int sumAbs, int count) {
	int mean = sumAbs / count;
	if (mean == 0) {
		return "silent";
	}
	return mean >= 16000 ? "full scale" : "partial";
}

static void report(int ch) {
	int zero[2] = {}, count[2] = {};
	int sumAbs[2] = {};
	// Adjacent frames of opposite sign: a 22.05khz tone flips on every frame, DC never does, and
	// a zero-stuffed signal never does either (every other frame is zero).
	int flips = 0;
	for (int i = 0; i < FRAMES; ++i) {
		int v = samples[i * 2 + ch];
		int parity = i & 1;
		count[parity]++;
		if (v == 0) {
			zero[parity]++;
		}
		sumAbs[parity] += v < 0 ? -v : v;
		if (i > 0 && v * samples[(i - 1) * 2 + ch] < 0) {
			flips++;
		}
	}
	checkpoint("  %c: even frames %s (zero %d/%d), odd frames %s (zero %d/%d), sign flips %d/%d", ch ? 'R' : 'L',
		describeLevel(sumAbs[0], count[0]), zero[0], count[0], describeLevel(sumAbs[1], count[1]), zero[1], count[1], flips, FRAMES - 1);
}

static void testCase(const char *title, int dry, int wet, int type, bool alternating) {
	fillPCM(alternating);

	__sceSasRevVON(&sasCore, dry, wet);
	__sceSasSetVolume(&sasCore, 0, 0x1000, 0x1000, 0x1000, 0x1000);
	__sceSasRevEVOL(&sasCore, 0x1000, 0x1000);
	__sceSasRevParam(&sasCore, 0, 0);
	__sceSasRevType(&sasCore, type);
	__sceSasSetKeyOff(&sasCore, 0);
	__sceSasSetADSRmode(&sasCore, 0, 15, PSP_SAS_ADSR_CURVE_MODE_LINEAR_INCREASE, PSP_SAS_ADSR_CURVE_MODE_DIRECT, PSP_SAS_ADSR_CURVE_MODE_DIRECT, PSP_SAS_ADSR_CURVE_MODE_DIRECT);
	__sceSasSetADSR(&sasCore, 0, 15, 0x40000000, 0x40000000, 0x40000000, 0x40000000);
	__sceSasSetGrain(&sasCore, GRAIN);
	__sceSasSetOutputmode(&sasCore, 0);
	__sceSasSetPitch(&sasCore, 0, 0x1000);
	__sceSasSetVoicePCM(&sasCore, 0, pcm, 256, 0);
	__sceSasSetKeyOn(&sasCore, 0);

	// Let the key on, the effect volume ramps and the reverb's delay lines settle.
	for (int i = 0; i < SETTLE_GRAINS; ++i) {
		__sceSasCore(&sasCore, scratch);
	}

	memset(samples, 0xcc, sizeof(samples));
	for (int i = 0; i < CAPTURE_GRAINS; ++i) {
		__sceSasCore(&sasCore, samples + i * GRAIN * 2);
	}

	checkpointNext(title);
	report(0);
	report(1);
}

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_SASCORE);

	__sceSasInit(&sasCore, GRAIN, 32, PSP_SAS_OUTPUTMODE_STEREO, 44100);

	// One section per case, so each one's output is flushed before the next starts. Buffering
	// whole sections lost the output on hardware.
	testCase("DC, wet only, ROOM:", 0, 1, PSP_SAS_EFFECT_TYPE_ROOM, false);
	testCase("DC, wet only, HALL:", 0, 1, PSP_SAS_EFFECT_TYPE_HALL, false);
	testCase("22.05khz tone, wet only, ROOM:", 0, 1, PSP_SAS_EFFECT_TYPE_ROOM, true);
	testCase("22.05khz tone, wet only, OFF:", 0, 1, PSP_SAS_EFFECT_TYPE_OFF, true);
	testCase("22.05khz tone, dry only, ROOM:", 1, 0, PSP_SAS_EFFECT_TYPE_ROOM, true);

	return 0;
}
