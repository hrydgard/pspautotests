// sceMpegBaseYCrCbCopyVme and sceMpegBaseCscVme, which the VSH's video player uses for Motion JPEG.
// The copy takes a descriptor of six buffers sized in pixels, and the conversion one sized in
// macroblocks: luma in two halves, then Cb and Cr for each half.
#include "shared.h"

extern int sceMpegBaseCscInit(int bufferWidth);
extern int sceMpegBaseYCrCbCopyVme(void *dst, void *src, int flags);
extern int sceMpegBaseCscVme(void *bufferRGB, void *bufferRGB2, int bufferWidth, void *csc);

typedef struct {
	int height;
	int width;
	int unk08;
	int unk0c;
	void *buffer[6];
} VmeStruct;

#define COPY_BYTES 0x2000

static void testCopy(int width, int height, int flags) {
	VmeStruct src, dst;
	memset(&src, 0, sizeof(src));
	memset(&dst, 0, sizeof(dst));
	src.height = height;
	src.width = width;
	src.unk0c = 1;
	int i;
	for (i = 0; i < 6; ++i) {
		src.buffer[i] = memalign(64, COPY_BYTES);
		dst.buffer[i] = memalign(64, COPY_BYTES);
		memset(src.buffer[i], 0x10 * (i + 1), COPY_BYTES);
		memset(dst.buffer[i], 0xCC, COPY_BYTES);
	}
	sceKernelDcacheWritebackInvalidateAll();

	int result = sceMpegBaseYCrCbCopyVme(&dst, &src, flags);
	sceKernelDcacheWritebackInvalidateAll();
	printf("copy %dx%d flags %d: %08x, bytes", width, height, flags, result);
	for (i = 0; i < 6; ++i) {
		const u8 *p = (const u8 *)dst.buffer[i];
		int n = 0;
		while (n < COPY_BYTES && p[n] == 0x10 * (i + 1)) {
			n++;
		}
		printf(" %d", n);
	}
	printf("\n");

	for (i = 0; i < 6; ++i) {
		free(src.buffer[i]);
		free(dst.buffer[i]);
	}
}

#define STRIDE 64
#define ROWS 80

static u8 *g_out;

// A 32x32 picture. Each luma row holds its own value, so the output shows which source row every
// row came from, and the chroma buffers differ so it shows which half they colour.
static void testCsc(int second) {
	VmeStruct csc;
	memset(&csc, 0, sizeof(csc));
	csc.height = 2;
	csc.width = 2;
	csc.unk0c = 1;
	static const int sizes[6] = {512, 512, 128, 128, 128, 128};
	static const u8 chroma[4] = {0x60, 0x80, 0xA0, 0x80};
	int i, row;
	for (i = 0; i < 6; ++i) {
		csc.buffer[i] = memalign(64, sizes[i]);
	}
	for (row = 0; row < 16; ++row) {
		memset((u8 *)csc.buffer[0] + row * 32, 0x20 + row * 4, 32);
		memset((u8 *)csc.buffer[1] + row * 32, 0x80 + row * 4, 32);
	}
	for (i = 0; i < 4; ++i) {
		memset(csc.buffer[i + 2], chroma[i], sizes[i + 2]);
	}
	memset(g_out, 0xCC, STRIDE * ROWS * 4);
	sceKernelDcacheWritebackInvalidateAll();

	// The second pointer: none, the same buffer, right after the top half (as the VSH's player
	// passes it), or further on.
	static const int secondRows[4] = {-1, 0, 16, 48};
	void *other = second == 0 ? NULL : g_out + secondRows[second] * STRIDE * 4;
	int result = sceMpegBaseCscVme(g_out, other, STRIDE, &csc);
	sceKernelDcacheWritebackInvalidateAll();
	printf("csc second=%d: %08x\n", secondRows[second], result);

	for (row = 0; row < ROWS; ++row) {
		const u8 *p = g_out + row * STRIDE * 4;
		int changed = 0, last = -1;
		int x;
		for (x = 0; x < STRIDE * 4; ++x) {
			if (p[x] != 0xCC) {
				changed++;
				last = x;
			}
		}
		if (changed == 0) {
			continue;
		}
		printf("  row %2d: %3d bytes changed, last at %3d:", row, changed, last);
		for (x = 0; x < 8; ++x) {
			printf(" %02x", p[x]);
		}
		printf(" |");
		for (x = 124; x < 132; ++x) {
			printf(" %02x", p[x]);
		}
		printf("\n");
	}

	for (i = 0; i < 6; ++i) {
		free(csc.buffer[i]);
	}
}

int main(int argc, char *argv[]) {
	if (loadVideoModules() < 0) {
		return 1;
	}
	g_out = (u8 *)memalign(64, STRIDE * ROWS * 4);

	sceMpegInit();
	int ringbufSize = sceMpegRingbufferQueryMemSize(64);
	int bufSize = sceMpegQueryMemSize(0);
	g_ringbufferData = malloc(ringbufSize);
	g_mpegData = malloc(bufSize);
	int result = sceMpegRingbufferConstruct((SceMpegRingbuffer *)&g_ringbuffer, 64, g_ringbufferData, ringbufSize, NULL, NULL);
	if (result == 0) {
		result = sceMpegCreate(&g_mpeg, g_mpegData, bufSize, (SceMpegRingbuffer *)&g_ringbuffer, 512, 0, 0);
	}
	printf("setup %08x\n", result);
	printf("sceMpegBaseCscInit %08x\n", sceMpegBaseCscInit(STRIDE));

	// Widths whose low seven bits are 0x50 get a different padding in mpegbase_260.prx.
	testCopy(32, 32, 1);
	testCopy(32, 32, 2);
	testCopy(32, 32, 3);
	testCopy(32, 32, 0);
	testCopy(48, 16, 3);
	testCopy(80, 16, 3);
	testCopy(64, 16, 3);
	// Too large for mpegbase_260.prx, which checks before it copies anything.
	testCopy(1024, 4096, 3);

	int second;
	for (second = 0; second < 4; ++second) {
		testCsc(second);
	}

	sceMpegDelete(&g_mpeg);
	sceMpegRingbufferDestruct((SceMpegRingbuffer *)&g_ringbuffer);
	free(g_ringbufferData);
	free(g_mpegData);
	free(g_out);
	unloadVideoModules();
	return 0;
}
