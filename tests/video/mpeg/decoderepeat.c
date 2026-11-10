// sceMpegAvcDecode called again on an access unit it has already decoded: what the call passes to
// the decoder, whether a held picture comes out, and how long it takes.
// Some movie players decode an access unit and, while no picture comes back, call
// sceMpegAvcDecode again on the same one without getting another.
#include "shared.h"

#define RING_PACKETS 512

static SceUID g_fd = -1;
static int g_eof;
static SceInt32 g_offset;
static void *g_frameBuf;
static void **g_frameBufHolder;

static SceInt32 ringCallback(void *data, SceInt32 numPackets, void *arg) {
	if (numPackets <= 0) {
		return 0;
	}
	int read = sceIoRead(g_fd, data, 2048 * numPackets);
	if (read <= 0) {
		g_eof = 1;
		return 0;
	}
	return read / 2048;
}

static const char *timeBucket(u32 us) {
	if (us < 500) {
		return "<0.5ms";
	}
	if (us < 2000) {
		return "0.5-2ms";
	}
	if (us < 8000) {
		return "2-8ms";
	}
	return ">=8ms";
}

static void restart() {
	sceMpegAvcDecodeFlush(&g_mpeg);
	sceMpegFlushAllStream(&g_mpeg);
	g_eof = 0;
	sceIoLseek(g_fd, g_offset, SEEK_SET);
}

static int nextAu() {
	int tries = 0;
	while (tries++ < 1000) {
		int avail = sceMpegRingbufferAvailableSize((SceMpegRingbuffer *)&g_ringbuffer);
		if (avail > 0 && !g_eof) {
			sceMpegRingbufferPut((SceMpegRingbuffer *)&g_ringbuffer, avail, avail);
		}
		SceInt32 attr = 0;
		int result = sceMpegGetAvcAu(&g_mpeg, g_avc_stream, &g_avc_au, &attr);
		if (result == 0) {
			return 0;
		}
		if (g_eof) {
			return result;
		}
	}
	return -1;
}

// Only the calls without an access unit are timed: how long a real decode takes is
// playertiming's business.
static void decode(const char *label) {
	SceMpegAu2 *au = (SceMpegAu2 *)&g_avc_au;
	u32 sizeBefore = au->auSize;
	SceInt32 status = -1;
	u32 start = sceKernelGetSystemTimeLow();
	int result = sceMpegAvcDecode(&g_mpeg, &g_avc_au, 512, g_frameBufHolder, &status);
	u32 took = sceKernelGetSystemTimeLow() - start;
	printf("  %s: %08x, status %d, auSize %s -> %s", label, result, (int)status,
		sizeBefore != 0 ? "nonzero" : "0", au->auSize != 0 ? "nonzero" : "0");
	if (sizeBefore == 0) {
		printf(", %s", timeBucket(took));
	}
	printf("\n");
}

// Decodes `fresh` access units in a row, then the last one again `repeats` times.
static void testRepeat(int fresh, int repeats) {
	printf("%d access unit(s), then the last again %d times:\n", fresh, repeats);
	restart();
	int i;
	for (i = 0; i < fresh; ++i) {
		int result = nextAu();
		if (result != 0) {
			printf("  sceMpegGetAvcAu: %08x\n", result);
			return;
		}
		char label[32];
		snprintf(label, sizeof(label), "AU %d", i);
		decode(label);
	}
	for (i = 0; i < repeats; ++i) {
		char label[32];
		snprintf(label, sizeof(label), "again #%d", i);
		decode(label);
	}
	// And whether the stream carries on normally afterwards.
	for (i = 0; i < 3; ++i) {
		int result = nextAu();
		if (result != 0) {
			printf("  sceMpegGetAvcAu: %08x\n", result);
			return;
		}
		char label[32];
		snprintf(label, sizeof(label), "AU %d", fresh + i);
		decode(label);
	}
}

// The same, with the access unit's size zeroed by hand rather than by a previous decode.
static void testZeroSize() {
	printf("a fresh access unit with auSize 0:\n");
	restart();
	if (nextAu() != 0) {
		return;
	}
	decode("AU 0");
	if (nextAu() != 0) {
		return;
	}
	((SceMpegAu2 *)&g_avc_au)->auSize = 0;
	decode("AU 1, size 0");
	decode("again #0");
}

int main(int argc, char *argv[]) {
	if (loadVideoModules() < 0) {
		return 1;
	}
	sceMpegInit();

	int ringbufSize = sceMpegRingbufferQueryMemSize(RING_PACKETS);
	int bufSize = sceMpegQueryMemSize(0);
	g_ringbufferData = malloc(ringbufSize);
	g_mpegData = malloc(bufSize);
	int result = sceMpegRingbufferConstruct((SceMpegRingbuffer *)&g_ringbuffer, RING_PACKETS, g_ringbufferData, ringbufSize, &ringCallback, NULL);
	if (result == 0) {
		result = sceMpegCreate(&g_mpeg, g_mpegData, bufSize, (SceMpegRingbuffer *)&g_ringbuffer, 512, 0, 0);
	}
	if (result != 0) {
		printf("TEST FAILURE: setup %08x\n", result);
		return 1;
	}

	g_avc_stream = sceMpegRegistStream(&g_mpeg, 0, 0);
	g_avc_buf = sceMpegMallocAvcEsBuf(&g_mpeg);
	sceMpegInitAu(&g_mpeg, g_avc_buf, &g_avc_au);

	g_fd = sceIoOpen("test.pmf", PSP_O_RDONLY, 0777);
	char header[2048];
	sceIoRead(g_fd, header, 2048);
	result = sceMpegQueryStreamOffset(&g_mpeg, header, &g_offset);
	printf("sceMpegQueryStreamOffset: %08x\n", result);

	g_frameBuf = memalign(64, 512 * 272 * 4);
	g_frameBufHolder = (void **)memalign(64, 512 * 272 * 4 + 64);
	g_frameBufHolder[0] = g_frameBuf;

	testRepeat(1, 4);
	testRepeat(3, 4);
	testZeroSize();

	free(g_frameBufHolder);
	free(g_frameBuf);
	sceIoClose(g_fd);
	sceMpegFreeAvcEsBuf(&g_mpeg, g_avc_buf);
	sceMpegUnRegistStream(&g_mpeg, g_avc_stream);
	sceMpegDelete(&g_mpeg);
	sceMpegRingbufferDestruct((SceMpegRingbuffer *)&g_ringbuffer);
	free(g_ringbufferData);
	free(g_mpegData);
	unloadVideoModules();
	return 0;
}
