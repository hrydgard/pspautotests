#include <common.h>
#include <pspaudio.h>
#include <pspthreadman.h>

static u16 data[128];

extern "C" int main(int argc, char *argv[]) {
	checkpointNext("Output2");
	checkpoint("  Reserve: %08x", sceAudioOutput2Reserve(64));
	checkpoint("  OutputBlocking: %08x", sceAudioOutput2OutputBlocking(0x7fff, data));
	checkpoint("  Rest: %08x", sceAudioOutput2GetRestSample());
	u64 t = sceKernelGetSystemTimeWide();
	while (true) {
		u64 n = sceKernelGetSystemTimeWide();
		int rest = sceAudioOutput2GetRestSample();
		if (rest != 0x40) {
			checkpoint("  Rest (after %dXXus): %08x", (int)(n - t) / 100, rest);
			break;
		}
		if (n > t + 10000) {
			break;
		}
	}
	sceAudioOutput2Release();
	checkpoint("  Not reserved: %08x", sceAudioOutput2GetRestSample());

	checkpointNext("SRC");
	checkpoint("  Reserve: %08x", sceAudioSRCChReserve(64, 44100, 2));
	checkpoint("  OutputBlocking: %08x", sceAudioSRCOutputBlocking(0x7fff, data));
	checkpoint("  Rest: %08x", sceAudioOutput2GetRestSample());
	t = sceKernelGetSystemTimeWide();
	while (true) {
		u64 n = sceKernelGetSystemTimeWide();
		int rest = sceAudioOutput2GetRestSample();
		if (rest != 0x40) {
			checkpoint("  Rest (after %dXXus): %08x", (int)(n - t) / 100, rest);
			break;
		}
		if (n > t + 10000) {
			break;
		}
	}
	sceAudioSRCChRelease();
	checkpoint("  Not reserved: %08x", sceAudioOutput2GetRestSample());

	return 0;
}