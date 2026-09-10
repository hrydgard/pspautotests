// Argument checking on the channel-configuration calls, which the emulator has
// historically guessed at. Nothing here needs a second thread - it is all about
// which error each call picks, and in what order it checks.

#include <common.h>
#include <pspaudio.h>
#include <pspthreadman.h>

static short data[8192 * 2];

static void testRelease() {
	checkpointNext("sceAudioChRelease");
	schedf("  never reserved: %08x\n", (int)sceAudioChRelease(3));
	schedf("  bad channel: %08x\n", (int)sceAudioChRelease(8));

	int chan = sceAudioChReserve(-1, 1024, PSP_AUDIO_FORMAT_STEREO);
	schedf("  reserved: %08x\n", (int)sceAudioChRelease(chan));
	schedf("  released twice: %08x\n", (int)sceAudioChRelease(chan));

	// Releasing while a buffer is still playing is allowed - only a parked thread
	// blocks it.
	chan = sceAudioChReserve(-1, 1024, PSP_AUDIO_FORMAT_STEREO);
	sceAudioOutputBlocking(chan, 0x7fff, data);
	schedf("  while playing: %08x\n", (int)sceAudioChRelease(chan));
	sceKernelDelayThread(100000);
}

static void testChangeConfig() {
	checkpointNext("sceAudioChangeChannelConfig");
	schedf("  not reserved: %08x\n", (int)sceAudioChangeChannelConfig(3, PSP_AUDIO_FORMAT_STEREO));
	schedf("  bad channel: %08x\n", (int)sceAudioChangeChannelConfig(8, PSP_AUDIO_FORMAT_STEREO));

	int chan = sceAudioChReserve(-1, 1024, PSP_AUDIO_FORMAT_STEREO);
	static const int formats[] = { -1, 0, 1, 2, 4, 8, 15, 16, 17, 32 };
	for (unsigned int i = 0; i < ARRAY_SIZE(formats); ++i) {
		schedf("  format %d: %08x\n", formats[i], (int)sceAudioChangeChannelConfig(chan, formats[i]));
	}
	schedf("  release: %08x\n", (int)sceAudioChRelease(chan));
}

static void testChangeVolume() {
	checkpointNext("sceAudioChangeChannelVolume");
	schedf("  not reserved: %08x\n", (int)sceAudioChangeChannelVolume(3, 0x4000, 0x4000));
	schedf("  bad channel: %08x\n", (int)sceAudioChangeChannelVolume(8, 0x4000, 0x4000));

	int chan = sceAudioChReserve(-1, 1024, PSP_AUDIO_FORMAT_STEREO);
	schedf("  too loud left: %08x\n", (int)sceAudioChangeChannelVolume(chan, 0x10000, 0x4000));
	schedf("  too loud right: %08x\n", (int)sceAudioChangeChannelVolume(chan, 0x4000, 0x10000));
	schedf("  negative left: %08x\n", (int)sceAudioChangeChannelVolume(chan, -1, 0x4000));
	schedf("  max: %08x\n", (int)sceAudioChangeChannelVolume(chan, 0xFFFF, 0xFFFF));
	schedf("  release: %08x\n", (int)sceAudioChRelease(chan));
}

static void testSetDataLen() {
	checkpointNext("sceAudioSetChannelDataLen");
	schedf("  not reserved: %08x\n", (int)sceAudioSetChannelDataLen(3, 1024));
	schedf("  not reserved, bad len: %08x\n", (int)sceAudioSetChannelDataLen(3, 1000));
	schedf("  bad channel: %08x\n", (int)sceAudioSetChannelDataLen(8, 1024));

	int chan = sceAudioChReserve(-1, 1024, PSP_AUDIO_FORMAT_STEREO);
	static const int lens[] = { -64, 0, 1, 63, 64, 1024, 65472, 65536 };
	for (unsigned int i = 0; i < ARRAY_SIZE(lens); ++i) {
		schedf("  len %d: %08x\n", lens[i], (int)sceAudioSetChannelDataLen(chan, lens[i]));
	}
	schedf("  release: %08x\n", (int)sceAudioChRelease(chan));
}

static void testOutput2ChangeLength() {
	checkpointNext("sceAudioOutput2ChangeLength");
	schedf("  not reserved: %08x\n", (int)sceAudioOutput2ChangeLength(1024));
	schedf("  not reserved, bad len: %08x\n", (int)sceAudioOutput2ChangeLength(0));

	schedf("  reserve: %08x\n", (int)sceAudioOutput2Reserve(1024));
	static const int lens[] = { -64, 0, 16, 17, 64, 1000, 4111, 4112, 65536 };
	for (unsigned int i = 0; i < ARRAY_SIZE(lens); ++i) {
		schedf("  len %d: %08x\n", lens[i], (int)sceAudioOutput2ChangeLength(lens[i]));
	}
	schedf("  release: %08x\n", (int)sceAudioOutput2Release());
}

static void testOutputVolume() {
	checkpointNext("Output volume limits");
	int chan = sceAudioChReserve(-1, 64, PSP_AUDIO_FORMAT_STEREO);
	schedf("  output 0x10000: %08x\n", (int)sceAudioOutput(chan, 0x10000, data));
	schedf("  output -1: %08x\n", (int)sceAudioOutput(chan, -1, data));
	schedf("  panned 0x10000/0: %08x\n", (int)sceAudioOutputPanned(chan, 0x10000, 0, data));
	schedf("  panned -1/0: %08x\n", (int)sceAudioOutputPanned(chan, -1, 0, data));
	schedf("  blocking -1: %08x\n", (int)sceAudioOutputBlocking(chan, -1, data));
	sceKernelDelayThread(50000);
	schedf("  panned blocking -1/0: %08x\n", (int)sceAudioOutputPannedBlocking(chan, -1, 0, data));
	sceKernelDelayThread(50000);
	schedf("  release: %08x\n", (int)sceAudioChRelease(chan));

	// Output2 and SRC take a wider volume range than the plain channels.
	schedf("  output2 reserve: %08x\n", (int)sceAudioOutput2Reserve(64));
	schedf("  output2 0xFFFFF: %08x\n", (int)sceAudioOutput2OutputBlocking(0xFFFFF, data));
	schedf("  output2 0x100000: %08x\n", (int)sceAudioOutput2OutputBlocking(0x100000, data));
	sceKernelDelayThread(50000);
	schedf("  output2 release: %08x\n", (int)sceAudioOutput2Release());
}

extern "C" int main(int argc, char *argv[]) {
	testRelease();
	testChangeConfig();
	testChangeVolume();
	testSetDataLen();
	testOutput2ChangeLength();
	testOutputVolume();

	flushschedf();
	return 0;
}
