// What the Media Engine writes beside a decoded picture, which games' mpeg.prx mostly passes through:
// sceMpegAvcDecodeDetail's output, the sceVideocodec context sceMpegAvcDecodeDetail2 hands back, the
// buffer at its +0x30, and the per-picture records the ME points at from each 0x2C-byte picture
// descriptor (+0x24 and +0x28). The VSH's UMD Video player reads those records.
//
// Pointers are printed relative to the mpeg buffer, and only followed when they're inside it.
#include "shared.h"

#define RING_PACKETS 512
#define AUS 6


static SceUID g_fd = -1;
static int g_eof;
static u32 g_ctx;
static u32 *g_records;

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

static int inMpegData(u32 addr, int size) {
	u32 base = (u32)g_mpegData & 0x3FFFFFFF;
	addr &= 0x3FFFFFFF;
	if (g_records && addr >= ((u32)g_records & 0x3FFFFFFF) && addr + size <= ((u32)g_records & 0x3FFFFFFF) + 0x540) {
		return 1;
	}
	return addr >= base && addr + size <= base + (u32)sceMpegQueryMemSize(0);
}

// A word that looks like a pointer into the mpeg buffer is printed as an offset into it.
static void printWord(u32 w) {
	if (g_records && w != 0 && (w & 0x3FFFFFFF) >= ((u32)g_records & 0x3FFFFFFF) && (w & 0x3FFFFFFF) < ((u32)g_records & 0x3FFFFFFF) + 0x540) {
		printf(" r+%03x", (unsigned)((w & 0x3FFFFFFF) - ((u32)g_records & 0x3FFFFFFF)));
	} else if (w != 0 && inMpegData(w, 4)) {
		printf(" m+%05x", (unsigned)((w & 0x3FFFFFFF) - ((u32)g_mpegData & 0x3FFFFFFF)));
	} else {
		printf(" %08x", (unsigned)w);
	}
}

static void dumpWords(const char *name, u32 addr, int size) {
	if (addr == 0 || !inMpegData(addr, size)) {
		printf("%s: not in the mpeg buffer:", name);
		printWord(addr);
		printf("\n");
		return;
	}
	const u32 *p = (const u32 *)addr;
	int i;
	for (i = 0; i < size / 4; i += 8) {
		printf("%s+%02x:", name, i * 4);
		int j;
		for (j = i; j < i + 8 && j < size / 4; ++j) {
			printWord(p[j]);
		}
		printf("\n");
	}
}

static void dumpPicture(u32 ctx) {
	const u32 *c = (const u32 *)ctx;
	dumpWords("  ctx", ctx, 0x60);
	if (!inMpegData(ctx, 0x60)) {
		return;
	}
	dumpWords("  ctx[30]", c[0x30 / 4], 0x64);
	u32 descs = c[0x2C / 4];
	int count = c[0x3C / 4];
	if (!inMpegData(descs, 0x2C * 4) || count <= 0 || count > 4) {
		printf("  descriptors: unexpected (count %d)\n", count);
		return;
	}
	int i;
	for (i = 0; i < count; ++i) {
		const u32 *d = (const u32 *)(descs + i * 0x2C);
		printf("  desc %d +20:", i);
		printWord(d[0x20 / 4]);
		printWord(d[0x24 / 4]);
		printWord(d[0x28 / 4]);
		printf("\n");
		char name[32];
		snprintf(name, sizeof(name), "   [24]");
		dumpWords(name, d[0x24 / 4], 0x40);
		snprintf(name, sizeof(name), "   [28]");
		dumpWords(name, d[0x28 / 4], 0x40);
	}
}

static void testDetail(int withRecords) {
	g_eof = 0;
	sceIoLseek(g_fd, g_streamOffset, SEEK_SET);
	sceMpegFlushAllStream(&g_mpeg);

	// mpeg_vsh.prx keeps the sceVideocodec context's address at +0x6C0 of its own state, which the
	// SceMpeg handle points at.
	u32 state = (u32)g_mpeg;
	if (!inMpegData(state + 0x6C0, 4)) {
		printf("mpeg state not in the mpeg buffer:");
		printWord(state);
		printf("\n");
		return;
	}
	g_ctx = *(u32 *)(state + 0x6C0);
	printf("ctx at state+6c0:");
	printWord(g_ctx);
	printf(", magic %08x\n", inMpegData(g_ctx, 4) ? (unsigned)*(u32 *)g_ctx : 0);
	if (!inMpegData(g_ctx, 0x60) || *(u32 *)g_ctx != 0x05100601) {
		return;
	}
	g_records = NULL;
	if (withRecords) {
		// What modes 3-6 give the codec: a 0x540-byte buffer at +0x38.
		g_records = (u32 *)memalign(64, 0x540);
		memset(g_records, 0, 0x540);
		sceKernelDcacheWritebackInvalidateRange(g_records, 0x540);
		*(u32 **)(g_ctx + 0x38) = g_records;
		sceKernelDcacheWritebackRange((void *)g_ctx, 0x60);
		printf("with a 0x540-byte buffer at ctx+38\n");
	}

	void *frameBuf = memalign(64, 512 * 272 * 4);
	void **frameBufHolder = (void **)memalign(64, 512 * 272 * 4 + 64);
	frameBufHolder[0] = frameBuf;

	int aus = 0;
	int iterations = 0;
	while (aus < AUS && iterations++ < 20000) {
		int avail = sceMpegRingbufferAvailableSize((SceMpegRingbuffer *)&g_ringbuffer);
		if (avail > 0 && !g_eof) {
			sceMpegRingbufferPut((SceMpegRingbuffer *)&g_ringbuffer, avail, avail);
		}

		SceInt32 attr = 0;
		if (sceMpegGetAvcAu(&g_mpeg, g_avc_stream, &g_avc_au, &attr) != 0) {
			if (g_eof) {
				break;
			}
			continue;
		}

		SceInt32 status = -1;
		int result = sceMpegAvcDecode(&g_mpeg, &g_avc_au, 512, frameBufHolder, &status);
		printf("AU %d: sceMpegAvcDecode %08x, status %d\n", aus, result, (int)status);
		aus++;
		if (result != 0) {
			break;
		}

		u32 detail[0x20];
		memset(detail, 0xCC, sizeof(detail));
		result = sceMpegAvcDecodeDetail(&g_mpeg, detail);
		printf(" sceMpegAvcDecodeDetail %08x:", result);
		int i;
		for (i = 0; i < 0x20; ++i) {
			if ((i & 7) == 0) {
				printf("\n  +%02x:", i * 4);
			}
			printWord(detail[i]);
		}
		printf("\n");
		dumpWords("  detail[2c]", detail[0x2C / 4], 0x40);
		dumpWords("  detail[30]", detail[0x30 / 4], 0x40);

		// Games' mpeg.prx doesn't have sceMpegAvcDecodeDetail2, so find the context where mpeg_vsh.prx
		// keeps it.
		dumpPicture(g_ctx);
		if (g_records) {
			dumpWords("  records", (u32)g_records, 0x540);
		}
	}

	if (g_records) {
		*(u32 *)(g_ctx + 0x38) = 0;
		free(g_records);
		g_records = NULL;
	}
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
	sceMpegQueryStreamOffset(&g_mpeg, header, &offset);
	g_streamOffset = offset;

	testDetail(0);
	testDetail(1);

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
