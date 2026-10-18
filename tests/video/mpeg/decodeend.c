// How many pictures the AVC decoder holds back: which sceMpegAvcDecode calls of a stream return
// one, and what sceMpegAvcDecodeStop hands back once the stream has run out.
//
// MGS Peace Walker's movie player only notices the end of a movie while its clock is short of the
// movie's length, so it hangs when the last picture arrives too late. Whether the hardware decoder
// keeps pictures back at the end decides that.
#include "shared.h"

#define RING_PACKETS 512
#define MAX_AUS 2000

static SceUID g_fd = -1;
static int g_eof;
static int g_status[MAX_AUS];
static u32 g_pts[MAX_AUS];

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

// Prints a run-length summary of the per-AU picture status, like "0x2 1x148".
static void printStatusRuns(int count) {
	int i = 0;
	printf("status runs:");
	while (i < count) {
		int j = i;
		while (j < count && g_status[j] == g_status[i]) {
			++j;
		}
		printf(" %dx%d", g_status[i], j - i);
		i = j;
	}
	printf("\n");
}

static void testDecodeEnd() {
	// Big enough whether a call takes the frame buffer itself or a pointer to it: the first word
	// points at the real buffer, and the rest leaves room for a whole picture.
	void *frameBuf = memalign(64, 512 * 272 * 4);
	void **frameBufHolder = (void **)memalign(64, 512 * 272 * 4 + 64);
	frameBufHolder[0] = frameBuf;

	int aus = 0;
	int pictures = 0;
	int firstPicture = -1;
	int iterations = 0;
	int lastError = 0;
	int i;

	while (aus < MAX_AUS && iterations++ < 20000) {
		int avail = sceMpegRingbufferAvailableSize((SceMpegRingbuffer *)&g_ringbuffer);
		if (avail > 0 && !g_eof) {
			int put = sceMpegRingbufferPut((SceMpegRingbuffer *)&g_ringbuffer, avail, avail);
			if (put < 0) {
				printf("sceMpegRingbufferPut: %08x\n", put);
				break;
			}
		}

		SceInt32 attr = 0;
		int result = sceMpegGetAvcAu(&g_mpeg, g_avc_stream, &g_avc_au, &attr);
		if (result != 0) {
			lastError = result;
			if (g_eof) {
				break;
			}
			continue;
		}

		SceInt32 status = -1;
		result = sceMpegAvcDecode(&g_mpeg, &g_avc_au, 512, frameBufHolder, &status);
		if (result != 0) {
			printf("sceMpegAvcDecode at AU %d: %08x\n", aus, result);
			break;
		}
		g_status[aus] = (int)status;
		g_pts[aus] = ((SceMpegAu2 *)&g_avc_au)->presentedTimeLow;
		if (status != 0) {
			if (firstPicture < 0) {
				firstPicture = aus;
			}
			pictures++;
		}
		aus++;
	}

	printf("access units %d, pictures %d, first picture at AU %d\n", aus, pictures, firstPicture);
	printf("end of stream: sceMpegGetAvcAu %08x\n", lastError);
	printStatusRuns(aus);
	int withPts = 0;
	int firstPts = -1;
	int lastPts = -1;
	for (i = 0; i < aus; ++i) {
		if (g_pts[i] != 0xFFFFFFFF) {
			if (withPts == 0) {
				firstPts = i;
			}
			lastPts = i;
			withPts++;
		}
	}
	printf("AUs with a pts: %d, first at AU %d, last at AU %d\n", withPts, firstPts, lastPts);
	if (withPts > 1) {
		printf("pts span %d\n", (int)(g_pts[lastPts] - g_pts[firstPts]));
	}

	// Drain whatever the decoder still holds.
	for (i = 0; i < 6; ++i) {
		SceInt32 status = -1;
		int result = sceMpegAvcDecodeStop(&g_mpeg, 512, frameBufHolder, &status);
		printf("sceMpegAvcDecodeStop #%d: %08x, status %d\n", i, result, (int)status);
	}
	printf("sceMpegAvcDecodeFlush: %08x\n", sceMpegAvcDecodeFlush(&g_mpeg));

	free(frameBufHolder);
	free(frameBuf);
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
	SceInt32 offset = 0;
	result = sceMpegQueryStreamOffset(&g_mpeg, header, &offset);
	printf("sceMpegQueryStreamOffset: %08x\n", result);
	sceIoLseek(g_fd, offset, SEEK_SET);

	testDecodeEnd();

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
