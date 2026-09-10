// sceVaudioChRelease is not the same shape as the Output2 and SRC releases. Those refuse while
// a buffer is still in flight; this one drains first - it hands the channel a null pointer,
// which waits for a buffer to finish, and only then releases. So it blocks rather than
// returning busy, and whatever was still playing is played rather than dropped.

#include <common.h>
#include <pspaudio.h>
#include <pspthreadman.h>
#include <psputility.h>

// sceaudio/shared.h declares these without C linkage, which only works from a .c file.
extern "C" {
int sceVaudioChReserve(int sampleCount, int freq, int channels);
int sceVaudioChRelease();
int sceVaudioOutputBlocking(int vol, void *buf);
}

static short data[8192 * 2];

// One buffer at 44100Hz, in milliseconds, is samples / 44.1.
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

static void testReleaseWhilePlaying() {
	checkpointNext("Release with a buffer still playing");
	schedf("  reserve: %08x\n", (int)sceVaudioChReserve(2048, 44100, 2));
	schedf("  output: %08x\n", (int)sceVaudioOutputBlocking(0x8000, data));
	schedf("  rest before release: %04x\n", (int)sceAudioOutput2GetRestSample());

	u64 t0 = sceKernelGetSystemTimeWide();
	int r = sceVaudioChRelease();
	u64 t1 = sceKernelGetSystemTimeWide();
	schedf("  release: %08x (%s)\n", r, waited(t0, t1, 2048));
	schedf("  rest after release: %08x\n", (int)sceAudioOutput2GetRestSample());
	schedf("  release again: %08x\n", (int)sceVaudioChRelease());
}

static void testReleaseWhenIdle() {
	checkpointNext("Release with nothing playing");
	schedf("  reserve: %08x\n", (int)sceVaudioChReserve(2048, 44100, 2));
	u64 t0 = sceKernelGetSystemTimeWide();
	int r = sceVaudioChRelease();
	u64 t1 = sceKernelGetSystemTimeWide();
	schedf("  release: %08x (%s)\n", r, waited(t0, t1, 2048));
}

static void testNotReserved() {
	checkpointNext("Release without reserving");
	schedf("  release: %08x\n", (int)sceVaudioChRelease());
}

// The channel is shared, so a vaudio reserve and an Output2 reserve fight over the same thing.
static void testSharedWithOutput2() {
	checkpointNext("Shared with Output2");
	schedf("  output2 reserve: %08x\n", (int)sceAudioOutput2Reserve(2048));
	schedf("  vaudio reserve: %08x\n", (int)sceVaudioChReserve(2048, 44100, 2));
	schedf("  output2 release: %08x\n", (int)sceAudioOutput2Release());
	schedf("  vaudio reserve after: %08x\n", (int)sceVaudioChReserve(2048, 44100, 2));
	schedf("  output: %08x\n", (int)sceVaudioOutputBlocking(0x8000, data));
	schedf("  vaudio release: %08x\n", (int)sceVaudioChRelease());
}

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_VAUDIO);

	testNotReserved();
	testReleaseWhenIdle();
	testReleaseWhilePlaying();
	testSharedWithOutput2();

	flushschedf();
	return 0;
}
