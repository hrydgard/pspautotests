// Two threads pushing to the same audio channel.
//
// The blocking output calls are not a queue that later callers line up behind.
// Only one thread can be waiting on a channel at a time; a second one is turned
// away with SCE_ERROR_AUDIO_CHANNEL_BUSY (0x80260002) straight away. Games that
// mix a movie thread and a sound-effect thread on the same channel depend on
// getting that error rather than being stalled.

#include <common.h>
#include <pspaudio.h>
#include <pspthreadman.h>

static short data[8192 * 2];

static int g_chan;
static volatile int g_helperResult;
static volatile int g_helperDone;

// Runs at a higher priority than main, so starting it hands control straight over
// and it is already parked inside the blocking call when main resumes.
static int channelHelper(SceSize args, void *argp) {
	g_helperResult = sceAudioOutputBlocking(g_chan, 0x7fff, data);
	g_helperDone = 1;
	return 0;
}

static int output2Helper(SceSize args, void *argp) {
	g_helperResult = sceAudioOutput2OutputBlocking(0x7fff, data);
	g_helperDone = 1;
	return 0;
}

static SceUID startHelper(SceKernelThreadEntry entry) {
	int prio = sceKernelGetThreadCurrentPriority() - 1;
	SceUID th = sceKernelCreateThread("helper", entry, prio, 0x1000, 0, NULL);
	g_helperResult = 0x7fffffff;
	g_helperDone = 0;
	sceKernelStartThread(th, 0, NULL);
	return th;
}

// "fast" means the call returned without waiting for a buffer to drain, which is
// the whole point - a stalled caller and a rejected caller look identical in the
// return value alone.
static const char *speed(u64 t0, u64 t1) {
	return (t1 - t0) < 5000 ? "fast" : "waited";
}

static void testChannel() {
	checkpointNext("Channel, second thread arrives while the first waits");
	g_chan = sceAudioChReserve(-1, 2048, PSP_AUDIO_FORMAT_STEREO);
	schedf("  reserve: %08x\n", g_chan);

	// Fills the one buffer slot the channel has. Returns without waiting.
	schedf("  first output: %08x\n", (int)sceAudioOutputBlocking(g_chan, 0x7fff, data));

	SceUID helper = startHelper(&channelHelper);
	schedf("  helper parked: done=%d\n", g_helperDone);

	u64 t0 = sceKernelGetSystemTimeWide();
	int r = sceAudioOutputBlocking(g_chan, 0x7fff, data);
	u64 t1 = sceKernelGetSystemTimeWide();
	schedf("  second blocking output: %08x (%s)\n", r, speed(t0, t1));

	t0 = sceKernelGetSystemTimeWide();
	r = sceAudioOutput(g_chan, 0x7fff, data);
	t1 = sceKernelGetSystemTimeWide();
	schedf("  non-blocking output: %08x (%s)\n", r, speed(t0, t1));

	// A waiting thread also locks out everything that would reconfigure the channel.
	schedf("  release: %08x\n", (int)sceAudioChRelease(g_chan));
	schedf("  setDataLen: %08x\n", (int)sceAudioSetChannelDataLen(g_chan, 1024));
	schedf("  changeConfig: %08x\n", (int)sceAudioChangeChannelConfig(g_chan, PSP_AUDIO_FORMAT_MONO));
	schedf("  changeVolume: %08x\n", (int)sceAudioChangeChannelVolume(g_chan, 0x4000, 0x4000));

	// Rest length counts the waiting thread's buffer as well as the playing one.
	int rest = sceAudioGetChannelRestLen(g_chan);
	schedf("  restLen with one waiting: %s\n", rest > 2048 ? "more than one buffer" : "one buffer or less");

	while (!g_helperDone) {
		sceKernelDelayThread(1000);
	}
	schedf("  helper result: %08x\n", g_helperResult);
	sceKernelWaitThreadEnd(helper, NULL);
	sceKernelDeleteThread(helper);

	sceKernelDelayThread(200000);
	schedf("  release when idle: %08x\n", (int)sceAudioChRelease(g_chan));
}

static void testOutput2() {
	checkpointNext("Output2, third buffer with no slot free");
	schedf("  reserve: %08x\n", (int)sceAudioOutput2Reserve(2048));

	// Arms the first of the two DMA descriptors and starts playback.
	schedf("  first output: %08x\n", (int)sceAudioOutput2OutputBlocking(0x7fff, data));

	// Arms the second descriptor, then parks waiting for the first to finish.
	SceUID helper = startHelper(&output2Helper);
	schedf("  helper parked: done=%d\n", g_helperDone);

	u64 t0 = sceKernelGetSystemTimeWide();
	int r = sceAudioOutput2OutputBlocking(0x7fff, data);
	u64 t1 = sceKernelGetSystemTimeWide();
	schedf("  third output: %08x (%s)\n", r, speed(t0, t1));

	while (!g_helperDone) {
		sceKernelDelayThread(1000);
	}
	schedf("  helper result: %08x\n", g_helperResult);
	sceKernelWaitThreadEnd(helper, NULL);
	sceKernelDeleteThread(helper);

	sceKernelDelayThread(200000);
	schedf("  release: %08x\n", (int)sceAudioOutput2Release());
}

// Passing a null pointer is the documented way to wait for a channel to drain.
static void testDrain() {
	checkpointNext("Null pointer drains");
	int chan = sceAudioChReserve(-1, 1024, PSP_AUDIO_FORMAT_STEREO);
	schedf("  reserve: %08x\n", chan);
	schedf("  drain when idle: %08x\n", (int)sceAudioOutputBlocking(chan, 0x7fff, 0));

	schedf("  output: %08x\n", (int)sceAudioOutputBlocking(chan, 0x7fff, data));
	u64 t0 = sceKernelGetSystemTimeWide();
	int r = sceAudioOutputBlocking(chan, 0x7fff, 0);
	u64 t1 = sceKernelGetSystemTimeWide();
	schedf("  drain when busy: %08x (%s)\n", r, speed(t0, t1));
	schedf("  restLen after drain: %04x\n", sceAudioGetChannelRestLen(chan));
	schedf("  release: %08x\n", (int)sceAudioChRelease(chan));
}

extern "C" int main(int argc, char *argv[]) {
	testChannel();
	testOutput2();
	testDrain();

	flushschedf();
	return 0;
}
