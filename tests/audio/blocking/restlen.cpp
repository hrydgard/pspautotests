// sceAudioGetChannelRestLen and sceAudioGetChannelRestLength are not the same
// function twice. Both add the reserved sample count when a thread is parked in
// a blocking output call, but only the "Length" one first checks that a buffer is
// actually playing - so after a null-pointer drain, which leaves the remaining
// count set but no buffer, the two disagree.
//
// sceAudioOutput2GetRestSample counts armed DMA descriptors instead, so it can
// report two buffers' worth.

#include <common.h>
#include <pspaudio.h>
#include <pspthreadman.h>

static short data[8192 * 2];

static int g_chan;
static volatile int g_helperDone;

static int channelHelper(SceSize args, void *argp) {
	sceAudioOutputBlocking(g_chan, 0x7fff, data);
	g_helperDone = 1;
	return 0;
}

static void both(const char *state) {
	schedf("  %-24s len=%08x length=%08x\n", state,
		sceAudioGetChannelRestLen(g_chan), sceAudioGetChannelRestLength(g_chan));
}

static void testChannel() {
	checkpointNext("Channel rest length");
	g_chan = 3;
	both("not reserved");

	g_chan = sceAudioChReserve(-1, 1024, PSP_AUDIO_FORMAT_STEREO);
	both("reserved, idle");

	sceAudioOutputBlocking(g_chan, 0x7fff, data);
	both("playing");

	sceKernelDelayThread(50000);
	both("drained");

	// A null pointer sets the remaining count but never arms a buffer.
	sceAudioOutputBlocking(g_chan, 0x7fff, 0);
	both("after null output");

	sceAudioOutputBlocking(g_chan, 0x7fff, data);
	g_helperDone = 0;
	SceUID helper = sceKernelCreateThread("helper", &channelHelper,
		sceKernelGetThreadCurrentPriority() - 1, 0x1000, 0, NULL);
	sceKernelStartThread(helper, 0, NULL);
	int len = sceAudioGetChannelRestLen(g_chan);
	int length = sceAudioGetChannelRestLength(g_chan);
	schedf("  %-24s len>1024=%d length>1024=%d\n", "one thread waiting", len > 1024, length > 1024);

	while (!g_helperDone) {
		sceKernelDelayThread(1000);
	}
	sceKernelWaitThreadEnd(helper, NULL);
	sceKernelDeleteThread(helper);
	sceKernelDelayThread(100000);
	schedf("  release: %08x\n", (int)sceAudioChRelease(g_chan));

	g_chan = 8;
	both("channel 8");
}

static void testOutput2() {
	checkpointNext("Output2 rest sample");
	schedf("  not reserved: %08x\n", (int)sceAudioOutput2GetRestSample());
	schedf("  reserve: %08x\n", (int)sceAudioOutput2Reserve(1024));
	schedf("  reserved, idle: %08x\n", (int)sceAudioOutput2GetRestSample());

	sceAudioOutput2OutputBlocking(0x7fff, data);
	schedf("  one buffer: %08x\n", (int)sceAudioOutput2GetRestSample());

	// The second call parks until the first descriptor retires, so by the time it
	// returns there is again exactly one armed. Shortening the buffer afterwards
	// changes what the count is reported in.
	schedf("  changeLength 64: %08x\n", (int)sceAudioOutput2ChangeLength(64));
	schedf("  after changeLength: %08x\n", (int)sceAudioOutput2GetRestSample());

	sceKernelDelayThread(200000);
	schedf("  release: %08x\n", (int)sceAudioOutput2Release());
}

extern "C" int main(int argc, char *argv[]) {
	testChannel();
	testOutput2();

	flushschedf();
	return 0;
}
