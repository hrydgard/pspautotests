// Mixer channel behavior that nothing else covers: when a released channel becomes available
// again, what a second channel starting up does to the first, mono buffers, and whether the
// high channels really are treated differently by the panned blocking output.

#include <common.h>
#include <pspaudio.h>
#include <pspthreadman.h>

static short data[8192 * 2];

static const char *cost(u64 t0, u64 t1) {
	const u64 us = t1 - t0;
	if (us < 10) {
		return "under 10us";
	}
	if (us < 100) {
		return "10-100us";
	}
	if (us < 1000) {
		return "100us-1ms";
	}
	return "over 1ms";
}

// A channel that has been released but is still playing out its last buffer. The automatic
// search is documented as skipping it; naming it outright is a different question.
static void testReserveWhileDraining() {
	checkpointNext("Reserving around a draining channel");
	int first = sceAudioChReserve(-1, 2048, PSP_AUDIO_FORMAT_STEREO);
	schedf("  first reserve: %08x\n", first);
	schedf("  output: %08x\n", (int)sceAudioOutputBlocking(first, 0x7fff, data));
	schedf("  release while playing: %08x\n", (int)sceAudioChRelease(first));

	int second = sceAudioChReserve(-1, 2048, PSP_AUDIO_FORMAT_STEREO);
	schedf("  next automatic reserve: %s\n", second == first ? "same channel" : "a different one");
	schedf("  release that one: %08x\n", (int)sceAudioChRelease(second));

	// Naming the draining channel explicitly only looks at whether it is reserved.
	schedf("  explicit reserve of it: %08x\n", (int)sceAudioChReserve(first, 2048, PSP_AUDIO_FORMAT_STEREO));
	schedf("  output to it: %08x\n", (int)sceAudioOutput(first, 0x7fff, data));
	schedf("  release: %08x\n", (int)sceAudioChRelease(first));

	// Once it has drained, the automatic search picks it up again.
	sceKernelDelayThread(200000);
	int again = sceAudioChReserve(-1, 2048, PSP_AUDIO_FORMAT_STEREO);
	schedf("  automatic reserve once drained: %s\n", again == first ? "same channel" : "a different one");
	schedf("  release: %08x\n", (int)sceAudioChRelease(again));
	sceKernelDelayThread(100000);
}

// Handing a buffer to the first channel starts the DMA, and the mixer takes a block out of it
// straight away. A second channel joining an already-running DMA should not get the same
// treatment - nothing has been read from its buffer when the call returns.
static void testSecondChannelStart() {
	checkpointNext("Second channel joining a running mixer");
	int a = sceAudioChReserve(-1, 2048, PSP_AUDIO_FORMAT_STEREO);
	int b = sceAudioChReserve(-1, 2048, PSP_AUDIO_FORMAT_STEREO);
	schedf("  reserved two: %s\n", a != b && a >= 0 && b >= 0 ? "yes" : "no");

	sceAudioOutputBlocking(a, 0x7fff, data);
	int restA = sceAudioGetChannelRestLen(a);
	schedf("  first channel after its output: %s\n", restA < 2048 ? "a block already gone" : "untouched");

	sceAudioOutputBlocking(b, 0x7fff, data);
	int restB = sceAudioGetChannelRestLen(b);
	schedf("  second channel after its output: %s\n", restB < 2048 ? "a block already gone" : "untouched");

	sceKernelDelayThread(200000);
	schedf("  release: %08x %08x\n", (int)sceAudioChRelease(a), (int)sceAudioChRelease(b));
}

// A mono buffer is half the bytes of a stereo one for the same sample count, but it should still
// take the same time to play and count down the same way.
static void testMono() {
	checkpointNext("Mono");
	int chan = sceAudioChReserve(-1, 2048, PSP_AUDIO_FORMAT_MONO);
	schedf("  reserve: %08x\n", chan);
	schedf("  output: %08x\n", (int)sceAudioOutputBlocking(chan, 0x7fff, data));

	u64 t0 = sceKernelGetSystemTimeWide();
	int first = sceAudioGetChannelRestLen(chan);
	int steps = 0;
	int step = 0;
	int uneven = 0;
	int last = first;
	u64 deadline = t0 + 500000;
	while (last > 0 && sceKernelGetSystemTimeWide() < deadline) {
		int rest = sceAudioGetChannelRestLen(chan);
		if (rest == last) {
			continue;
		}
		int thisStep = last - rest;
		if (steps == 0) {
			step = thisStep;
		} else if (thisStep != step) {
			uneven++;
		}
		steps++;
		last = rest;
	}
	u64 t1 = sceKernelGetSystemTimeWide();
	const int expected = 2048 * 1000000 / 44100;
	const int pct = (int)(t1 - t0) * 100 / expected;
	schedf("  first=%04x steps=%d of %d, uneven=%d\n", first, steps, step, uneven);
	schedf("  drained in %s\n", pct > 80 && pct < 120 ? "about one buffer" : "some other time");

	sceKernelDelayThread(100000);
	schedf("  release: %08x\n", (int)sceAudioChRelease(chan));
}

// sceAudioOutputPannedBlocking reads a byte the other output calls don't and, if the channel
// number is high enough, delays by 3ms. Nothing in the module writes that byte, so this should
// come out the same on every channel - recorded so a firmware where it doesn't is noticed.
static void testPannedHighChannels() {
	checkpointNext("Panned blocking, low and high channels");
	static const int channels[] = { 0, 4, 5, 6, 7 };
	for (unsigned int i = 0; i < ARRAY_SIZE(channels); ++i) {
		int chan = sceAudioChReserve(channels[i], 2048, PSP_AUDIO_FORMAT_STEREO);
		u64 t0 = sceKernelGetSystemTimeWide();
		int r = sceAudioOutputPannedBlocking(chan, 0x7fff, 0x7fff, data);
		u64 t1 = sceKernelGetSystemTimeWide();
		schedf("  channel %d: %08x (%s)\n", channels[i], r, cost(t0, t1));
		sceKernelDelayThread(100000);
		sceAudioChRelease(chan);
	}
}

extern "C" int main(int argc, char *argv[]) {
	testReserveWhileDraining();
	testSecondChannelStart();
	testMono();
	testPannedHighChannels();

	flushschedf();
	return 0;
}
