// Times __sceSasCore across the things that could change its cost: grain size, number of playing
// voices, voice type, pitch, reverb and output mode, at the default and the highest clocks. Timing
// probe, not a pass/fail test.
//
// The mix runs on the Media Engine, which the video decoder shares, so how long it takes matters
// for movie players in games that keep SAS running (Star Wars: Lethal Alliance).
#include <common.h>

#include <pspkernel.h>
#include <psppower.h>
#include <psputility.h>

#include <stdio.h>
#include <string.h>

#include "../sascore/sascore.h"

// The test harness echoes stdout to the debug screen; it doesn't matter here, but it costs time.
extern unsigned int HAS_DISPLAY;

enum { TYPE_VAG, TYPE_PCM, TYPE_NOISE };

typedef struct {
	const char *name;
	int grain;
	int voices;
	int type;
	int pitch;
	int revType;  // -1 = off
	int outMode;
	int wetWithoutReverb;  // wet on even with reverb type -1
} Config;

static const Config configs[] = {
	// Voices, VAG, the common case.
	{ "vag x0 grain512", 512, 0, TYPE_VAG, 0x1000, -1, 0 },
	{ "vag x1 grain512", 512, 1, TYPE_VAG, 0x1000, -1, 0 },
	{ "vag x2 grain512", 512, 2, TYPE_VAG, 0x1000, -1, 0 },
	{ "vag x4 grain512", 512, 4, TYPE_VAG, 0x1000, -1, 0 },
	{ "vag x8 grain512", 512, 8, TYPE_VAG, 0x1000, -1, 0 },
	{ "vag x16 grain512", 512, 16, TYPE_VAG, 0x1000, -1, 0 },
	{ "vag x32 grain512", 512, 32, TYPE_VAG, 0x1000, -1, 0 },
	// Voice type.
	{ "pcm x2 grain512", 512, 2, TYPE_PCM, 0x1000, -1, 0 },
	{ "pcm x8 grain512", 512, 8, TYPE_PCM, 0x1000, -1, 0 },
	{ "pcm x32 grain512", 512, 32, TYPE_PCM, 0x1000, -1, 0 },
	{ "noise x8 grain512", 512, 8, TYPE_NOISE, 0x1000, -1, 0 },
	// Grain size.
	{ "vag x0 grain64", 64, 0, TYPE_VAG, 0x1000, -1, 0 },
	{ "vag x0 grain256", 256, 0, TYPE_VAG, 0x1000, -1, 0 },
	{ "vag x0 grain1024", 1024, 0, TYPE_VAG, 0x1000, -1, 0 },
	{ "vag x0 grain2048", 2048, 0, TYPE_VAG, 0x1000, -1, 0 },
	{ "vag x8 grain64", 64, 8, TYPE_VAG, 0x1000, -1, 0 },
	{ "vag x8 grain256", 256, 8, TYPE_VAG, 0x1000, -1, 0 },
	{ "vag x8 grain1024", 1024, 8, TYPE_VAG, 0x1000, -1, 0 },
	{ "vag x8 grain2048", 2048, 8, TYPE_VAG, 0x1000, -1, 0 },
	{ "vag x32 grain2048", 2048, 32, TYPE_VAG, 0x1000, -1, 0 },
	// Pitch (resampling).
	{ "vag x8 pitch 0x800", 512, 8, TYPE_VAG, 0x800, -1, 0 },
	{ "vag x8 pitch 0x2000", 512, 8, TYPE_VAG, 0x2000, -1, 0 },
	{ "vag x8 pitch 0x4000", 512, 8, TYPE_VAG, 0x4000, -1, 0 },
	{ "vag x8 pitch 743", 512, 8, TYPE_VAG, 743, -1, 0 },
	// Reverb.
	{ "vag x2 reverb 0", 512, 2, TYPE_VAG, 0x1000, 0, 0 },
	{ "vag x2 reverb 1", 512, 2, TYPE_VAG, 0x1000, 1, 0 },
	{ "vag x2 reverb 4", 512, 2, TYPE_VAG, 0x1000, 4, 0 },
	{ "vag x2 reverb 7", 512, 2, TYPE_VAG, 0x1000, 7, 0 },
	{ "vag x0 reverb 7", 512, 0, TYPE_VAG, 0x1000, 7, 0 },
	{ "vag x32 reverb 7", 512, 32, TYPE_VAG, 0x1000, 7, 0 },
	// Output mode (1 = raw, no mixdown to stereo).
	{ "vag x8 outmode 1", 512, 8, TYPE_VAG, 0x1000, -1, 1 },
	{ "vag x2 reverb -1, wet on", 512, 2, TYPE_VAG, 0x1000, -1, 0, 1 },
	{ "vag x0 reverb -1, wet on", 512, 0, TYPE_VAG, 0x1000, -1, 0, 1 },
};

#define REPEATS 24
#define PCM_SAMPLES 4096
#define VAG_BYTES 8192

static SasCore sasCore __attribute__((aligned(64)));
static short pcm[PCM_SAMPLES] __attribute__((aligned(64)));
static u8 vag[VAG_BYTES] __attribute__((aligned(64)));
static short out[2048 * 4] __attribute__((aligned(64)));

static void makeData() {
	int i;
	for (i = 0; i < PCM_SAMPLES; i++) {
		pcm[i] = (short)(((i * 7) & 255) * 64 - 8192);
	}
	// ADPCM blocks of 16 bytes: filter/shift, flags, 14 bytes of nibbles. The content doesn't
	// matter for the cost; the flags make it loop from the first block.
	for (i = 0; i < VAG_BYTES; i += 16) {
		int j;
		vag[i] = 0x1A;
		vag[i + 1] = i == 0 ? 6 : (i + 16 >= VAG_BYTES ? 3 : 2);
		for (j = 2; j < 16; j++) {
			vag[i + j] = (u8)((i * 31 + j * 17) * 2654435761u >> 24);
		}
	}
	sceKernelDcacheWritebackAll();
}

static int runConfig(const Config *c, int *minUs) {
	memset(&sasCore, 0, sizeof(sasCore));
	int r = __sceSasInit(&sasCore, c->grain, 32, c->outMode, 44100);
	if (r != 0) {
		printf("%-24s sceSasInit %08x\n", c->name, r);
		return -1;
	}
	__sceSasRevType(&sasCore, c->revType);
	__sceSasRevEVOL(&sasCore, 0x1000, 0x1000);
	__sceSasRevVON(&sasCore, 1, c->revType >= 0 || c->wetWithoutReverb ? 1 : 0);
	int v;
	for (v = 0; v < c->voices; v++) {
		switch (c->type) {
		case TYPE_VAG: __sceSasSetVoice(&sasCore, v, vag, VAG_BYTES, 1); break;
		case TYPE_PCM: __sceSasSetVoicePCM(&sasCore, v, pcm, PCM_SAMPLES, 1); break;
		case TYPE_NOISE: __sceSasSetNoise(&sasCore, v, 16); break;
		}
		__sceSasSetPitch(&sasCore, v, c->pitch);
		__sceSasSetVolume(&sasCore, v, 0x800, 0x800, 0x800, 0x800);
		// Instant attack, full sustain, no release while keyed on.
		__sceSasSetADSR(&sasCore, v, 15, 0x40000000, 0, 0x7FFFFFFF, 0);
		__sceSasSetKeyOn(&sasCore, v);
	}

	int i, sum = 0;
	*minUs = 0x7FFFFFFF;
	for (i = 0; i < 4; i++) {
		__sceSasCore(&sasCore, out);
	}
	for (i = 0; i < REPEATS; i++) {
		u32 t0 = sceKernelGetSystemTimeLow();
		__sceSasCore(&sasCore, out);
		int us = (int)(sceKernelGetSystemTimeLow() - t0);
		sum += us;
		if (us < *minUs) *minUs = us;
	}
	for (v = 0; v < c->voices; v++) {
		__sceSasSetKeyOff(&sasCore, v);
	}
	return sum / REPEATS;
}

int main(int argc, char *argv[]) {
	HAS_DISPLAY = 0;
	if (sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC) < 0 || sceUtilityLoadModule(PSP_MODULE_AV_SASCORE) < 0) {
		printf("Could not load the modules\n");
	}
	makeData();

	static const int clocks[2][3] = { { 222, 222, 111 }, { 333, 333, 166 } };
	int k, c;
	for (k = 0; k < 2; k++) {
		scePowerSetClockFrequency(clocks[k][0], clocks[k][1], clocks[k][2]);
		printf("clocks: cpu %d, bus %d\n", scePowerGetCpuClockFrequencyInt(), scePowerGetBusClockFrequencyInt());
		printf("%-24s %8s %8s\n", "config", "min us", "avg us");
		for (c = 0; c < (int)(sizeof(configs) / sizeof(configs[0])); c++) {
			int minUs;
			int avg = runConfig(&configs[c], &minUs);
			if (avg >= 0) {
				printf("%-24s %8d %8d\n", configs[c].name, minUs, avg);
			}
		}
	}
	scePowerSetClockFrequency(222, 222, 111);
	return 0;
}
