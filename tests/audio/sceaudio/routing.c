#include "shared.h"

int sceAudioRoutingGetMode();
int sceAudioRoutingGetVolumeMode();
int sceAudioRoutingSetMode(int mode);
int sceAudioRoutingSetVolumeMode(int mode);

int main(int argc, char *argv[]) {
	int i;
	const int origMode = sceAudioRoutingGetMode();
	const int origVolumeMode = sceAudioRoutingGetVolumeMode();
	checkpointNext("Initial:");
	checkpoint("  Mode: %08x", origMode);
	checkpoint("  Volume mode: %08x", origVolumeMode);

	const static int modes[] = {0, 1, 2, 3, 4, 7, 8, 0x100, 0x7FFFFFFF, -1, 0};
	checkpointNext("sceAudioRoutingSetVolumeMode:");
	for (i = 0; i < ARRAY_SIZE(modes); ++i) {
		int result = sceAudioRoutingSetVolumeMode(modes[i]);
		checkpoint("  %d: %08x, now %08x (mode %08x)", modes[i], result, sceAudioRoutingGetVolumeMode(), sceAudioRoutingGetMode());
	}

	checkpointNext("sceAudioRoutingSetMode:");
	for (i = 0; i < ARRAY_SIZE(modes); ++i) {
		int result = sceAudioRoutingSetMode(modes[i]);
		checkpoint("  %d: %08x, now %08x (volume mode %08x)", modes[i], result, sceAudioRoutingGetMode(), sceAudioRoutingGetVolumeMode());
	}

	sceAudioRoutingSetMode(origMode);
	sceAudioRoutingSetVolumeMode(origVolumeMode);
	return 0;
}
