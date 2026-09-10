// What an audio output call costs when it does not wait for a buffer.
//
// The emulator charges a flat 10000 cycles - about 45us - to every Output2 and SRC output,
// including the ones that immediately answer "busy". A game whose losing thread retries in a
// tight loop feels that directly, so it is worth knowing what the hardware actually spends.
//
// Times are bucketed, and the first line is the cost of the measurement itself so the noise
// floor is on the record.

#include <common.h>
#include <pspaudio.h>
#include <pspthreadman.h>

static short data[8192 * 2];

static int g_chan;
static volatile int g_helperDone;

static const char *cost(u64 t0, u64 t1) {
	const u64 us = t1 - t0;
	if (us < 10) {
		return "under 10us";
	}
	if (us < 30) {
		return "10-30us";
	}
	if (us < 100) {
		return "30-100us";
	}
	return "over 100us";
}

static int output2Helper(SceSize args, void *argp) {
	sceAudioOutput2OutputBlocking(0x7fff, data);
	g_helperDone = 1;
	return 0;
}

static int channelHelper(SceSize args, void *argp) {
	sceAudioOutputBlocking(g_chan, 0x7fff, data);
	g_helperDone = 1;
	return 0;
}

static SceUID startHelper(SceKernelThreadEntry entry) {
	SceUID th = sceKernelCreateThread("helper", entry, sceKernelGetThreadCurrentPriority() - 1, 0x1000, 0, NULL);
	g_helperDone = 0;
	sceKernelStartThread(th, 0, NULL);
	return th;
}

static void waitHelper(SceUID th) {
	while (!g_helperDone) {
		sceKernelDelayThread(1000);
	}
	sceKernelWaitThreadEnd(th, NULL);
	sceKernelDeleteThread(th);
}

static void testBaseline() {
	checkpointNext("Baseline");
	u64 t0 = sceKernelGetSystemTimeWide();
	u64 t1 = sceKernelGetSystemTimeWide();
	schedf("  reading the clock twice: %s\n", cost(t0, t1));
}

static void testOutput2() {
	checkpointNext("Output2");
	schedf("  reserve: %08x\n", (int)sceAudioOutput2Reserve(2048));

	// The first output after an idle stretch is the one that starts playback, so it returns
	// without waiting and its cost is all setup.
	u64 t0 = sceKernelGetSystemTimeWide();
	int r = sceAudioOutput2OutputBlocking(0x7fff, data);
	u64 t1 = sceKernelGetSystemTimeWide();
	schedf("  first output: %08x (%s)\n", r, cost(t0, t1));

	// With both descriptors armed the answer is busy, and nothing is set up at all.
	SceUID helper = startHelper(&output2Helper);
	t0 = sceKernelGetSystemTimeWide();
	r = sceAudioOutput2OutputBlocking(0x7fff, data);
	t1 = sceKernelGetSystemTimeWide();
	schedf("  refused output: %08x (%s)\n", r, cost(t0, t1));
	waitHelper(helper);

	sceKernelDelayThread(200000);
	schedf("  release: %08x\n", (int)sceAudioOutput2Release());

	// Not reserved at all is the cheapest path there is.
	t0 = sceKernelGetSystemTimeWide();
	r = sceAudioOutput2OutputBlocking(0x7fff, data);
	t1 = sceKernelGetSystemTimeWide();
	schedf("  unreserved output: %08x (%s)\n", r, cost(t0, t1));
}

static void testChannel() {
	checkpointNext("Channel");
	g_chan = sceAudioChReserve(-1, 2048, PSP_AUDIO_FORMAT_STEREO);
	schedf("  reserve: %08x\n", g_chan);

	u64 t0 = sceKernelGetSystemTimeWide();
	int r = sceAudioOutput(g_chan, 0x7fff, data);
	u64 t1 = sceKernelGetSystemTimeWide();
	schedf("  first output: %08x (%s)\n", r, cost(t0, t1));

	t0 = sceKernelGetSystemTimeWide();
	r = sceAudioOutput(g_chan, 0x7fff, data);
	t1 = sceKernelGetSystemTimeWide();
	schedf("  refused output: %08x (%s)\n", r, cost(t0, t1));

	// A blocking output turned away because another thread is already parked.
	SceUID helper = startHelper(&channelHelper);
	t0 = sceKernelGetSystemTimeWide();
	r = sceAudioOutputBlocking(g_chan, 0x7fff, data);
	t1 = sceKernelGetSystemTimeWide();
	schedf("  refused blocking output: %08x (%s)\n", r, cost(t0, t1));
	waitHelper(helper);

	sceKernelDelayThread(200000);
	schedf("  release: %08x\n", (int)sceAudioChRelease(g_chan));
}

extern "C" int main(int argc, char *argv[]) {
	testBaseline();
	testOutput2();
	testChannel();

	flushschedf();
	return 0;
}
