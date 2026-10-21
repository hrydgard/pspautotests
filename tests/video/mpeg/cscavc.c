// What sceMpegBaseCscAvc writes for each setting of the descriptor's words at +0x08 and +0x0C,
// which the VSH's UMD Video player sets to 1 and 1. A 32x32 frame (2x2 macroblocks) whose four
// luma buffers each hold their own value, so the output shows which buffer every pixel came from,
// converted into a buffer prefilled with 0xCC, so it also shows how many rows and bytes get written.
#include "shared.h"

extern int sceMpegBaseCscInit(int bufferWidth);
extern int sceMpegBaseCscAvc(void *bufferRGB, void *unknown, int bufferWidth, void *csc);

typedef struct {
	int height;  // macroblocks
	int width;
	int unk08;
	int unk0c;
	void *buffer[8];
} CscStruct;

#define STRIDE 64
#define ROWS 72

static u8 *g_out;

static void testMode(int unk08, int unk0c, int vram, int second) {
	// The luma buffers are 256 bytes each for a 32x32 frame, chroma 128.
	static const int sizes[8] = {256, 256, 256, 256, 128, 128, 128, 128};
	static const u8 values[8] = {0x30, 0x60, 0x90, 0xC0, 0x80, 0x80, 0x80, 0x80};
	CscStruct csc;
	memset(&csc, 0, sizeof(csc));
	csc.height = 2;
	csc.width = 2;
	csc.unk08 = unk08;
	csc.unk0c = unk0c;
	int i;
	for (i = 0; i < 8; ++i) {
		csc.buffer[i] = memalign(64, sizes[i]);
		memset(csc.buffer[i], values[i], sizes[i]);
		sceKernelDcacheWritebackRange(csc.buffer[i], sizes[i]);
	}
	sceKernelDcacheWritebackRange(&csc, sizeof(csc));

	// The VSH's player converts into VRAM; read it back through the uncached mirror.
	u8 *out = vram ? (u8 *)((u32)sceGeEdramGetAddr() + 0x100000) : g_out;
	u8 *read = vram ? (u8 *)((u32)out | 0x40000000) : g_out;
	memset(read, 0xCC, STRIDE * ROWS * 4);
	sceKernelDcacheWritebackInvalidateAll();

	// The second pointer: none, the same buffer (as the VSH's player passes), or 32 rows further on.
	void *other = second == 0 ? NULL : (second == 1 ? out : out + 32 * STRIDE * 4);
	int result = sceMpegBaseCscAvc(out, other, STRIDE, &csc);
	sceKernelDcacheWritebackInvalidateAll();
	printf("%s second=%d +08=%d +0C=%d: sceMpegBaseCscAvc %08x\n", vram ? "VRAM" : "RAM", second, unk08, unk0c, result);

	// For each row: how many bytes changed and the last one that did, and the first bytes.
	int row;
	for (row = 0; row < ROWS; ++row) {
		const u8 *p = read + row * STRIDE * 4;
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
		for (x = 32; x < 40; ++x) {
			printf(" %02x", p[x]);
		}
		printf(" |");
		for (x = 64; x < 72; ++x) {
			printf(" %02x", p[x]);
		}
		printf("\n");
	}

	for (i = 0; i < 8; ++i) {
		free(csc.buffer[i]);
	}
}

int main(int argc, char *argv[]) {
	if (loadVideoModules() < 0) {
		return 1;
	}
	g_out = (u8 *)memalign(64, STRIDE * ROWS * 4);

	// Games only convert once mpeg.prx has set the decoder up.
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

	int vram, second;
	for (vram = 0; vram < 2; ++vram) {
		for (second = 0; second < 3; ++second) {
			testMode(0, 0, vram, second);
			testMode(1, 0, vram, second);
			testMode(0, 1, vram, second);
			testMode(1, 1, vram, second);
		}
	}

	sceMpegDelete(&g_mpeg);
	sceMpegRingbufferDestruct((SceMpegRingbuffer *)&g_ringbuffer);
	free(g_ringbufferData);
	free(g_mpegData);
	free(g_out);
	unloadVideoModules();
	return 0;
}
