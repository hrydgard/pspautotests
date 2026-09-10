// How deep is the queue behind the blocking audio output calls, and how much does each call
// actually wait for?
//
// The first call after an idle stretch returns straight away, because handing over the first
// buffer is what starts playback. After that each call waits for exactly one buffer to finish,
// so one buffer is playing and one is queued and no more.
//
// Waits are reported as a fraction of a buffer rather than in milliseconds, since the exact
// microsecond a call returns on is not the point and does not reproduce.

#include <common.h>
#include <pspaudio.h>
#include <pspthreadman.h>

static short data[8192 * 2];

static const char *waited(u64 t0, u64 t1, int samples) {
	const int expected = samples * 1000000 / 44100;
	const int pct = (int)(t1 - t0) * 100 / expected;
	if (pct < 25) {
		return "no wait";
	}
	if (pct < 150) {
		return "one buffer";
	}
	return "longer";
}

static void probeOutput2(int samples) {
	schedf("Output2 %d samples: reserve=%08x\n", samples, (int)sceAudioOutput2Reserve(samples));
	for (int i = 0; i < 6; i++) {
		u64 t0 = sceKernelGetSystemTimeWide();
		int r = sceAudioOutput2OutputBlocking(0x7fff, data);
		u64 t1 = sceKernelGetSystemTimeWide();
		schedf("  [%d] ret=%08x %s rest=%04x\n", i, r, waited(t0, t1, samples), sceAudioOutput2GetRestSample());
	}
	sceKernelDelayThread(200000);
	schedf("  release=%08x\n", (int)sceAudioOutput2Release());
}

static void probeSRC(int samples, int freq) {
	schedf("SRC %d samples @%d: reserve=%08x\n", samples, freq, (int)sceAudioSRCChReserve(samples, freq, 2));
	for (int i = 0; i < 6; i++) {
		u64 t0 = sceKernelGetSystemTimeWide();
		int r = sceAudioSRCOutputBlocking(0x7fff, data);
		u64 t1 = sceKernelGetSystemTimeWide();
		schedf("  [%d] ret=%08x %s rest=%04x\n", i, r, waited(t0, t1, samples), sceAudioOutput2GetRestSample());
	}
	sceKernelDelayThread(200000);
	schedf("  release=%08x\n", (int)sceAudioSRCChRelease());
}

static void probeChannel(int samples) {
	int chan = sceAudioChReserve(-1, samples, PSP_AUDIO_FORMAT_STEREO);
	schedf("Channel %d samples: reserve=%08x\n", samples, chan);
	for (int i = 0; i < 6; i++) {
		u64 t0 = sceKernelGetSystemTimeWide();
		int r = sceAudioOutputBlocking(chan, 0x7fff, data);
		u64 t1 = sceKernelGetSystemTimeWide();
		schedf("  [%d] ret=%08x %s rest=%04x\n", i, r, waited(t0, t1, samples), sceAudioGetChannelRestLen(chan));
	}
	sceKernelDelayThread(200000);
	schedf("  release=%08x\n", (int)sceAudioChRelease(chan));
}

// The non-blocking form takes one buffer and then says busy - there is no queue for it to fill.
static void probeChannelNonBlocking(int samples) {
	int chan = sceAudioChReserve(-1, samples, PSP_AUDIO_FORMAT_STEREO);
	schedf("Channel (non-blocking) %d samples: reserve=%08x\n", samples, chan);
	for (int i = 0; i < 6; i++) {
		int r = sceAudioOutput(chan, 0x7fff, data);
		schedf("  [%d] ret=%08x\n", i, r);
	}
	sceKernelDelayThread(200000);
	schedf("  release=%08x\n", (int)sceAudioChRelease(chan));
}

// The driver takes a fixed block out of the buffer each time the DMA asks for one, so the
// remaining count walks down in equal steps rather than sliding smoothly. Polled as fast as
// possible so no step is missed.
static void probeRestSteps(int samples) {
	int chan = sceAudioChReserve(-1, samples, PSP_AUDIO_FORMAT_STEREO);
	schedf("Rest steps, %d samples: reserve=%08x\n", samples, chan);
	sceAudioOutputBlocking(chan, 0x7fff, data);

	int first = sceAudioGetChannelRestLen(chan);
	int last = first;
	int steps = 0;
	int step = 0;
	int uneven = 0;
	u64 deadline = sceKernelGetSystemTimeWide() + 500000;
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
	schedf("  first=%04x steps=%d of %d, uneven=%d, ended at %04x\n", first, steps, step, uneven, last);

	sceKernelDelayThread(100000);
	schedf("  release=%08x\n", (int)sceAudioChRelease(chan));
}

extern "C" int main(int argc, char *argv[]) {
	probeOutput2(1024);
	probeSRC(1024, 44100);
	probeChannel(1024);
	probeChannelNonBlocking(1024);
	probeRestSteps(1024);

	flushschedf();
	return 0;
}
