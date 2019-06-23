#include <common.h>
#include <pspaudio.h>
#include <pspthreadman.h>

static u16 data[128];

extern "C" int main(int argc, char *argv[]) {
	checkpointNext("Output2");
	checkpoint("  Reserve: %08x", sceAudioOutput2Reserve(64));
	checkpoint("  OutputBlocking: %08x", sceAudioOutput2OutputBlocking(0x7fff, data));
	checkpoint("  Release: %08x", sceAudioOutput2Release());
	sceKernelDelayThread(1000);
	checkpoint("  Release (after 1ms): %08x", sceAudioOutput2Release());
	sceKernelDelayThread(1000);
	checkpoint("  Release (after 2ms): %08x", sceAudioOutput2Release());
	checkpoint("  Release (again): %08x", sceAudioOutput2Release());

	checkpointNext("SRC");
	checkpoint("  Reserve: %08x", sceAudioSRCChReserve(64, 44100, 2));
	checkpoint("  OutputBlocking: %08x", sceAudioSRCOutputBlocking(0x7fff, data));
	checkpoint("  Release: %08x", sceAudioSRCChRelease());
	sceKernelDelayThread(1000);
	checkpoint("  Release (after 1ms): %08x", sceAudioSRCChRelease());
	sceKernelDelayThread(1000);
	checkpoint("  Release (after 2ms): %08x", sceAudioSRCChRelease());
	checkpoint("  Release (again): %08x", sceAudioSRCChRelease());

	return 0;
}