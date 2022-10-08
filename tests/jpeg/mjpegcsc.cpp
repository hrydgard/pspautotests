#include "shared.h"
#include <malloc.h>
#include <psputility.h>
#include <psputils.h>

extern "C" int sceJpegMJpegCsc(void *dest, const void *srcYCbCr, uint32_t widthHeight, int stride);

static uint8_t ycbcr[1572864];
static uint32_t *frame;
static const uint32_t FRAME_SIZE = 1024 * 1024 * 4;

static void testJpegCsc(const char *title, bool useDest, bool useSrc, uint32_t widthHeight, int stride) {
	memset(frame, 0xCC, FRAME_SIZE + 4);
	sceKernelDcacheWritebackInvalidateRange(frame, FRAME_SIZE + 4);

	int result = sceJpegMJpegCsc(useDest ? frame : NULL, useSrc ? ycbcr : NULL, widthHeight, stride);
	size_t len = 0;
	for (int i = 1024 * 1024; i >= 0; --i) {
		if (frame[i] != 0xCCCCCCCC) {
			len = (i + 1) * sizeof(uint32_t);
			break;
		}
	}
	size_t rowLen = 0;
	for (int i = 0; i < (int)len; ++i) {
		if (frame[i] == 0xCCCCCCCC) {
			rowLen = i * sizeof(uint32_t);
			break;
		}
	}
	checkpoint("%s: %08x (wrote %d bytes, %d per row)", title, result, len, rowLen);
	if (!useSrc) {
		checkpoint("%s: wrote %08x", title, frame[0]);
	}
}

static void testJpegCscPattern(const char *title, int y, int cb, int cr) {
	sceKernelDcacheWritebackInvalidateRange(frame, FRAME_SIZE);
	memset(frame, 0xCC, FRAME_SIZE);
	sceKernelDcacheWritebackInvalidateRange(frame, FRAME_SIZE);

	int w = 480;
	int h = 272;
	int ysize = w * h;
	int cbcrSize = ysize >> 2;

	sceKernelDcacheWritebackInvalidateRange(ycbcr, sizeof(ycbcr));
	memset(ycbcr, 0, sizeof(ycbcr));
	memset(ycbcr, 0xFF, ysize - 256 + y);
	memset(ycbcr + ysize, 0x80, cbcrSize - 64 + cb);
	memset(ycbcr + ysize + cbcrSize, 0x80, cbcrSize - 64 + cr);
	sceKernelDcacheWritebackInvalidateRange(ycbcr, sizeof(ycbcr));

	sceKernelDcacheWritebackInvalidateAll();
	int result = sceJpegMJpegCsc(frame, ycbcr, (w << 16) | h, w);
	sceKernelDcacheWritebackInvalidateAll();
	checkpoint(NULL);
	schedf("%s: %08x =", title, result);
	uint32_t last = frame[0];
	int lasti = 0;
	for (int i = 1; i < w * h; ++i) {
		if (frame[i] != last) {
			schedf(" %d-%d %08x", lasti, i - 1, last);
			last = frame[i];
			lasti = i;
		}
	}
	schedf(" %d-%d %08x\n", lasti, w * h - 1, last);
}

static void testJpegCscValue(const char *title, uint8_t yy, uint8_t cb, uint8_t cr) {
	sceKernelDcacheWritebackInvalidateRange(frame, 4);
	memset(frame, 0xCC, 4);
	sceKernelDcacheWritebackInvalidateRange(frame, 4);

	sceKernelDcacheWritebackInvalidateRange(ycbcr, sizeof(ycbcr));
	memset(ycbcr, 0, 768);
	ycbcr[0] = yy;
	ycbcr[256] = cb;
	ycbcr[256 + 64] = cr;
	sceKernelDcacheWritebackInvalidateRange(ycbcr, sizeof(ycbcr));

	int result = sceJpegMJpegCsc(frame, ycbcr, 0x00100010, 0x0010);
	checkpoint("%s: %08x (%08x)", title, result, frame[0]);
}

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);

	frame = (uint32_t *)malloc(FRAME_SIZE + 4);

	checkpointNext("Basic:");
	testJpegCsc("  Before init", true, true, 0x00100010, 0x0010);
	checkpoint("  Init: %08x", sceJpegInitMJpeg());
	testJpegCsc("  After init", true, true, 0x00100010, 0x0010);
	checkpoint("  Finish: %08x", sceJpegFinishMJpeg());
	testJpegCsc("  After finish", true, true, 0x00100010, 0x0010);
	sceJpegInitMJpeg();

	checkpointNext("Buffers:");
	testJpegCsc("  Output NULL", false, true, 0x00100010, 0x0010);
	testJpegCsc("  Input NULL", true, false, 0x00100010, 0x0010);

	checkpointNext("Sizes:");
	testJpegCsc("  Wider than stride", true, true, 0x00200020, 0x0010);
	testJpegCsc("  Thinner than stride", true, true, 0x00100020, 0x0020);
	testJpegCsc("  Odd size", true, true, 0x05550555, 0x0010);
	testJpegCsc("  Large width", true, true, 0xFFFF0001, 0x0010);
	testJpegCsc("  721 width", true, true, (721 << 16) | 1, 0x0010);
	testJpegCsc("  720 width", true, true, (720 << 16) | 1, 0x0010);
	testJpegCsc("  719 width", true, true, (719 << 16) | 1, 0x0010);
	testJpegCsc("  720x32", true, true, (720 << 16) | 32, 0x0010);
	testJpegCsc("  720x16", true, true, (720 << 16) | 16, 0x0010);
	testJpegCsc("  720x8", true, true, (720 << 16) | 8, 0x0010);
	testJpegCsc("  Zero width", true, true, 0x00000010, 0x0010);
	testJpegCsc("  Small width", true, true, 0x00070010, 0x0010);
	testJpegCsc("  Large height", true, true, 0x0008FFFF, 0x0008);
	testJpegCsc("  481 height", true, true, 0x00080000 | 481, 0x0008);
	testJpegCsc("  480 height", true, true, 0x00080000 | 480, 0x0008);
	testJpegCsc("  479 height", true, true, 0x00080000 | 479, 0x0008);
	testJpegCsc("  Zero height", true, true, 0x00100000, 0x0010);

	checkpointNext("Strides:");
	// Hangs.
	//testJpegCsc("  Negative stride", true, true, 0x00100010, -1);
	testJpegCsc("  Negative stride", true, true, 0x00100010, -4);
	testJpegCsc("  Zero stride", true, true, 0x00100010, 0);
	testJpegCsc("  One stride", true, true, 0x00100010, 1);
	testJpegCsc("  1024 stride", true, true, 0x00100010, 1024);
	testJpegCsc("  1025 stride", true, true, 0x00100010, 1025);
	testJpegCsc("  Large stride", true, true, 0x00100002, 0x7FFFFFFF);
	testJpegCsc("  Very negative stride", true, true, 0x00100002, 0x80000000);

	checkpointNext("X/Y Pattern:");
	testJpegCscPattern("  2x2 full", 256, 64, 64);
	testJpegCscPattern("  2x2 short y", 255, 64, 64);
	testJpegCscPattern("  2x2 short cb", 256, 63, 64);
	testJpegCscPattern("  2x2 short cr", 256, 64, 63);

	checkpointNext("Bits:");
	testJpegCscValue("  2x2 Black", 0, 128, 128);
	testJpegCscValue("  2x2 White", 255, 128, 128);
	testJpegCscValue("  2x2 0 Cb", 255, 0, 128);
	testJpegCscValue("  2x2 1 Cb", 255, 1, 128);
	testJpegCscValue("  2x2 254 Cb", 255, 254, 128);
	testJpegCscValue("  2x2 255 Cb", 255, 255, 128);
	testJpegCscValue("  2x2 0 Cr", 255, 128, 0);
	testJpegCscValue("  2x2 1 Cr", 255, 128, 1);
	testJpegCscValue("  2x2 254 Cr", 255, 128, 254);
	testJpegCscValue("  2x2 255 Cr", 255, 128, 255);

	free(frame);
	sceUtilityUnloadModule(PSP_MODULE_AV_AVCODEC);
	return 0;
}