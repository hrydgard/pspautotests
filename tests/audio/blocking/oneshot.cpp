// sceAudioOneshotOutput plays a single buffer on a channel without reserving it. The channel
// is never marked as taken, so it frees itself when the buffer runs out, and the usual
// reserve/release pair is skipped entirely.
//
// Nothing is known to use it, which is why the emulator left it unimplemented - this records
// what it does so that stops being a guess.

#include <common.h>
#include <pspaudio.h>
#include <pspthreadman.h>

static short data[8192 * 2];

extern "C" int sceAudioOneshotOutput(int chan, int sampleCount, int format, int volLeft, int volRight, void *buf);

static void testBasic() {
	checkpointNext("Basic");
	int r = sceAudioOneshotOutput(0, 1024, PSP_AUDIO_FORMAT_STEREO, 0x7fff, 0x7fff, data);
	schedf("  oneshot on channel 0: %08x\n", r);
	schedf("  rest right after: %04x\n", sceAudioGetChannelRestLen(0));

	// It never reserved anything, so releasing has nothing to release.
	schedf("  release: %08x\n", (int)sceAudioChRelease(0));
	// And a reserve of the same channel is accepted even while it plays out.
	schedf("  reserve while playing: %08x\n", (int)sceAudioChReserve(0, 1024, PSP_AUDIO_FORMAT_STEREO));
	schedf("  output while playing: %08x\n", (int)sceAudioOutput(0, 0x7fff, data));
	schedf("  release: %08x\n", (int)sceAudioChRelease(0));
	sceKernelDelayThread(200000);

	// The automatic search only takes it back once it has drained.
	int auto1 = sceAudioChReserve(-1, 1024, PSP_AUDIO_FORMAT_STEREO);
	schedf("  automatic reserve after draining: %08x\n", auto1);
	schedf("  release: %08x\n", (int)sceAudioChRelease(auto1));
}

static void testAutoChannel() {
	checkpointNext("Automatic channel");
	int r = sceAudioOneshotOutput(-1, 1024, PSP_AUDIO_FORMAT_STEREO, 0x7fff, 0x7fff, data);
	schedf("  oneshot on channel -1: %08x\n", r);
	sceKernelDelayThread(200000);
}

static void testOnReservedChannel() {
	checkpointNext("On a reserved channel");
	int chan = sceAudioChReserve(3, 1024, PSP_AUDIO_FORMAT_STEREO);
	schedf("  reserve: %08x\n", chan);
	schedf("  oneshot on it: %08x\n", sceAudioOneshotOutput(3, 1024, PSP_AUDIO_FORMAT_STEREO, 0x7fff, 0x7fff, data));
	schedf("  release: %08x\n", (int)sceAudioChRelease(3));
}

static void testTwice() {
	checkpointNext("Twice in a row");
	schedf("  first: %08x\n", sceAudioOneshotOutput(1, 2048, PSP_AUDIO_FORMAT_STEREO, 0x7fff, 0x7fff, data));
	schedf("  second while it plays: %08x\n", sceAudioOneshotOutput(1, 2048, PSP_AUDIO_FORMAT_STEREO, 0x7fff, 0x7fff, data));
	sceKernelDelayThread(200000);
}

static void testArguments() {
	checkpointNext("Channels");
	static const int channels[] = { -2, -1, 0, 7, 8, 9 };
	for (unsigned int i = 0; i < ARRAY_SIZE(channels); ++i) {
		schedf("  %d: %08x\n", channels[i], sceAudioOneshotOutput(channels[i], 64, PSP_AUDIO_FORMAT_STEREO, 0x7fff, 0x7fff, data));
		sceKernelDelayThread(5000);
	}
	sceKernelDelayThread(100000);

	checkpointNext("Sample counts");
	static const int counts[] = { -64, 0, 1, 63, 64, 100, 1024, 65472, 65536 };
	for (unsigned int i = 0; i < ARRAY_SIZE(counts); ++i) {
		schedf("  %d: %08x\n", counts[i], sceAudioOneshotOutput(2, counts[i], PSP_AUDIO_FORMAT_STEREO, 0x7fff, 0x7fff, data));
		sceKernelDelayThread(5000);
	}
	sceKernelDelayThread(200000);

	checkpointNext("Formats");
	static const int formats[] = { -1, 0, 1, 2, 15, 16, 17 };
	for (unsigned int i = 0; i < ARRAY_SIZE(formats); ++i) {
		schedf("  %d: %08x\n", formats[i], sceAudioOneshotOutput(2, 64, formats[i], 0x7fff, 0x7fff, data));
		sceKernelDelayThread(5000);
	}
	sceKernelDelayThread(100000);

	checkpointNext("Volumes");
	schedf("  left 0x10000: %08x\n", sceAudioOneshotOutput(2, 64, PSP_AUDIO_FORMAT_STEREO, 0x10000, 0x7fff, data));
	schedf("  right 0x10000: %08x\n", sceAudioOneshotOutput(2, 64, PSP_AUDIO_FORMAT_STEREO, 0x7fff, 0x10000, data));
	schedf("  left -1: %08x\n", sceAudioOneshotOutput(2, 64, PSP_AUDIO_FORMAT_STEREO, -1, 0x7fff, data));
	sceKernelDelayThread(100000);

	checkpointNext("Null buffer");
	schedf("  null: %08x\n", sceAudioOneshotOutput(2, 64, PSP_AUDIO_FORMAT_STEREO, 0x7fff, 0x7fff, 0));
	sceKernelDelayThread(100000);
}

extern "C" int main(int argc, char *argv[]) {
	testBasic();
	testAutoChannel();
	testOnReservedChannel();
	testTwice();
	testArguments();

	flushschedf();
	return 0;
}
