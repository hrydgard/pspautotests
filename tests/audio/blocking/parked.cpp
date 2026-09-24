// Once a thread parked in a blocking output gets its buffer in, has any of it been played by the
// time the call returns? From idle it has: the mixer outranks the caller and takes the first 64
// samples right away (see restlen). This is the steady state, where the call waited for the
// previous buffer to finish. Fired Up rewrites the first 16 samples right after the call returns,
// so it matters which.

#include <common.h>
#include <pspaudio.h>
#include <pspthreadman.h>

static short data[2][1024 * 2];

extern "C" int main(int argc, char *argv[]) {
	int chan = sceAudioChReserve(-1, 1024, PSP_AUDIO_FORMAT_STEREO);
	schedf("reserve: %s\n", chan >= 0 ? "ok" : "failed");

	sceAudioOutputBlocking(chan, 0x7fff, data[0]);
	schedf("from idle: rest %08x\n", sceAudioGetChannelRestLen(chan));

	for (int i = 0; i < 6; ++i) {
		sceAudioOutputBlocking(chan, 0x7fff, data[(i + 1) & 1]);
		const int rest = sceAudioGetChannelRestLen(chan);
		schedf("after waiting, %d: rest %08x\n", i, rest);
	}

	// The same buffer again each time, as the game does.
	for (int i = 0; i < 3; ++i) {
		sceAudioOutputBlocking(chan, 0x7fff, data[0]);
		schedf("same buffer, %d: rest %08x\n", i, sceAudioGetChannelRestLen(chan));
	}

	sceKernelDelayThread(100000);
	schedf("release: %08x\n", sceAudioChRelease(chan));
	flushschedf();
	return 0;
}
