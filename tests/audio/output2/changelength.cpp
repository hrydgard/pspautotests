#include <common.h>
#include <pspaudio.h>
#include <pspthreadman.h>

static u16 data[8192];

extern "C" int main(int argc, char *argv[]) {
	checkpointNext("Output2");
	checkpoint("  Reserve: %08x", sceAudioOutput2Reserve(4096));
	checkpoint("  OutputBlocking: %08x", sceAudioOutput2OutputBlocking(0x7fff, data));
	checkpoint("  Length to 64: %08x", sceAudioOutput2ChangeLength(64));
	checkpoint("  Rest: %08x", sceAudioOutput2GetRestSample());
	sceKernelDelayThread(1000);
	checkpoint("  Length to 64 (after 1ms): %08x", sceAudioOutput2ChangeLength(64));
	sceKernelDelayThread(3000);
	checkpoint("  Length to 64 (after 4ms): %08x, rest %08x", sceAudioOutput2ChangeLength(64), sceAudioOutput2GetRestSample());
	while (sceAudioOutput2Release() != 0) {
		sceKernelDelayThread(500);
	}
	checkpoint("  Released");
	checkpoint("  Not reserved: %08x", sceAudioOutput2ChangeLength(64));

	checkpointNext("SRC");
	checkpoint("  Reserve: %08x", sceAudioSRCChReserve(4096, 44100, 2));
	checkpoint("  OutputBlocking: %08x", sceAudioSRCOutputBlocking(0x7fff, data));
	checkpoint("  Length to 64: %08x", sceAudioOutput2ChangeLength(64));
	checkpoint("  Rest: %08x", sceAudioOutput2GetRestSample());
	sceKernelDelayThread(1000);
	checkpoint("  Length to 64 (after 1ms): %08x", sceAudioOutput2ChangeLength(64));
	sceKernelDelayThread(3000);
	checkpoint("  Length to 64 (after 4ms): %08x, rest %08x", sceAudioOutput2ChangeLength(64), sceAudioOutput2GetRestSample());
	while (sceAudioSRCChRelease() != 0) {
		sceKernelDelayThread(500);
	}
	checkpoint("  Released");
	checkpoint("  Not reserved: %08x", sceAudioOutput2ChangeLength(64));

	return 0;
}