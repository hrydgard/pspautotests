// How long the calls that tear down a video player take, and whether they let other threads run:
// sceMpegFreeAvcEsBuf, sceMpegDelete and sceMpegRingbufferDestruct, on a fresh mpeg, one with its
// streams registered, and one that has decoded some frames.

#include "shared.h"

// Times are put in coarse ranges, so they can be compared with an emulator's.
static const char *timeRange(SceInt64 us) {
#ifdef EXACT_TIMES
	static char buf[32];
	snprintf(buf, sizeof(buf), "%lld us", us);
	return buf;
#else
	if (us < 1000)
		return "under 1 ms";
	if (us < 5000)
		return "1-5 ms";
	if (us < 20000)
		return "5-20 ms";
	if (us < 60000)
		return "20-60 ms";
	return "60 ms or more";
#endif
}

#define TIMED(label, call) do { \
		SceInt64 start = sceKernelGetSystemTimeWide(); \
		int result = call; \
		SceInt64 took = sceKernelGetSystemTimeWide() - start; \
		checkpoint("  %s: %08x, %s", label, result, timeRange(took)); \
	} while (0)

static void decodeFrames(int frames) {
	void *vbuffer = memalign(64, 512 * 272 * 4);
	void *abuffer;
	SceInt32 avcParam = 6;
	SceInt32 decodeParam = 512;
	int j;
	for (j = 0; j < frames; ++j) {
		int freePackets = sceMpegRingbufferAvailableSize((SceMpegRingbuffer *) &g_ringbuffer);
		sceMpegRingbufferPut((SceMpegRingbuffer *) &g_ringbuffer, 24, freePackets);
		sceMpegGetAtracAu(&g_mpeg, g_atrac_stream, &g_atrac_au, &abuffer);
		sceMpegGetAvcAu(&g_mpeg, g_avc_stream, &g_avc_au, &avcParam);
		SceInt32 *decodeParamp = &decodeParam;
		sceMpegAvcDecode(&g_mpeg, &g_avc_au, 512, &vbuffer, (SceInt32 *) &decodeParamp);
	}
	free(vbuffer);
}

static void teardown(const char *title, int registered) {
	checkpointNext(title);
	if (registered) {
		TIMED("sceMpegFreeAvcEsBuf", (sceMpegFreeAvcEsBuf(&g_mpeg, g_avc_buf), 0));
	}
	TIMED("sceMpegDelete", sceMpegDelete(&g_mpeg));
	TIMED("sceMpegRingbufferDestruct", sceMpegRingbufferDestruct((SceMpegRingbuffer *) &g_ringbuffer));
	free(g_mpegData);
	g_mpegData = NULL;
	free(g_ringbufferData);
	g_ringbufferData = NULL;
	free(g_atracData);
	g_atracData = NULL;
}

int main(int argc, char *argv[]) {
	if (loadVideoModules() < 0) {
		return 1;
	}
	sceMpegInit();

	if (createTestMpeg(512) >= 0) {
		teardown("Fresh:", 0);
	}

	if (createTestMpeg(512) >= 0) {
		registMpegStreams(0, 0);
		teardown("Streams registered:", 1);
	}

	if (createTestMpeg(512) >= 0) {
		registMpegStreams(0, 0);
		loadMpegFile("test.pmf");
		decodeFrames(10);
		teardown("After decoding:", 1);
		sceIoClose(g_mpegFile);
	}

	unloadVideoModules();
	return 0;
}
