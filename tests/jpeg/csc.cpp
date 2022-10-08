#include "shared.h"
#include <malloc.h>
#include <psputility.h>
#include <psputils.h>

// sceJpegCsc
extern "C" int sceJpeg_67F0ED84(void *dest, const void *srcYCbCr, uint32_t widthHeight, int stride, uint32_t chroma);

static uint8_t ycbcr[786432];
static uint32_t *frame;
static const uint32_t FRAME_SIZE = 1024 * 1024 * 4;

static void testJpegCsc(const char *title, bool useDest, bool useSrc, uint32_t widthHeight, int stride, uint32_t chroma) {
	memset(frame, 0xCC, FRAME_SIZE + 4);
	sceKernelDcacheWritebackInvalidateRange(frame, FRAME_SIZE + 4);

	uint32_t result = sceJpeg_67F0ED84(useDest ? frame : NULL, useSrc ? ycbcr : NULL, widthHeight, stride, chroma);
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
}

static void testJpegCscPattern(const char *title, int chroma, int y, int cb, int cr) {
	sceKernelDcacheWritebackInvalidateRange(frame, 256);
	memset(frame, 0xCC, 256);
	sceKernelDcacheWritebackInvalidateRange(frame, 256);

	int cbcrSize = chroma == 0x00020101 ? 256 : (chroma == 0x00020201 ? 128 : 64);

	sceKernelDcacheWritebackInvalidateRange(ycbcr, sizeof(ycbcr));
	memset(ycbcr, 0, 768);
	memset(ycbcr, 0xFF, y);
	memset(ycbcr + 256, 0xFF, cb);
	memset(ycbcr + 256 + cbcrSize, 0xFF, cr);
	sceKernelDcacheWritebackInvalidateRange(ycbcr, 768);

	int result = sceJpeg_67F0ED84(frame, ycbcr, 0x00100010, 0x0010, chroma);
	checkpoint(NULL);
	schedf("%s: %08x =", title, result);
	uint32_t last = frame[0];
	int lasti = 0;
	for (int i = 1; i < 256; ++i) {
		if (frame[i] != last) {
			schedf(" %d-%d %08x", lasti, i - 1, last);
			last = frame[i];
			lasti = i;
		}
	}
	schedf(" %d-255 %08x\n", lasti, last);
}

static void testJpegCscValue(const char *title, int chroma, uint8_t yy, uint8_t cb, uint8_t cr) {
	sceKernelDcacheWritebackInvalidateRange(frame, 4);
	memset(frame, 0xCC, 4);
	sceKernelDcacheWritebackInvalidateRange(frame, 4);

	sceKernelDcacheWritebackInvalidateRange(ycbcr, sizeof(ycbcr));
	memset(ycbcr, 0, 768);
	ycbcr[0] = yy;
	ycbcr[256] = cb;
	if (chroma == 0x00020202) {
		ycbcr[256 + 64] = cr;
	} else if (chroma == 0x00020201) {
		ycbcr[256 + 128] = cr;
	} else if (chroma == 0x00020101) {
		ycbcr[256 + 256] = cr;
	}
	sceKernelDcacheWritebackInvalidateRange(ycbcr, sizeof(ycbcr));

	int result = sceJpeg_67F0ED84(frame, ycbcr, 0x00100010, 0x0010, chroma);
	checkpoint("%s: %08x (%08x)", title, result, frame[0]);
}

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);

	frame = (uint32_t *)malloc(FRAME_SIZE + 4);

	checkpointNext("Basic:");
	testJpegCsc("  Before init", true, true, 0x00100010, 0x0010, 0x00020202);
	checkpoint("  Init: %08x", sceJpegInitMJpeg());
	testJpegCsc("  After init", true, true, 0x00100010, 0x0010, 0x00020202);

	checkpointNext("Buffers:");
	testJpegCsc("  Output NULL", false, true, 0x00100010, 0x0010, 0x00020202);
	testJpegCsc("  input NULL", true, false, 0x00100010, 0x0010, 0x00020202);

	checkpointNext("Sizes:");
	testJpegCsc("  Wider than stride", true, true, 0x00200020, 0x0010, 0x00020202);
	testJpegCsc("  Thinner than stride", true, true, 0x00100020, 0x0020, 0x00020202);
	testJpegCsc("  Odd size", true, true, 0x05550555, 0x0010, 0x00020202);
	testJpegCsc("  Large width", true, true, 0xFFFF0001, 0x0010, 0x00020202);
	// Seems like values below 7 (but 7 DOES work) hang.
	//testJpegCsc("  Zero width", true, true, 0x00000010, 0x0010, 0x00020202);
	testJpegCsc("  Small width", true, true, 0x00070010, 0x0010, 0x00020202);
	testJpegCsc("  Large height", true, true, 0x0008FFFF, 0x0008, 0x00020202);
	testJpegCsc("  Zero height", true, true, 0x00080000, 0x0008, 0x00020202);
	testJpegCsc("  One height", true, true, 0x00080001, 0x0008, 0x00020202);

	checkpointNext("Strides:");
	testJpegCsc("  Zero stride", true, true, 0x00100010, 0, 0x00020202);
	testJpegCsc("  One stride", true, true, 0x00100010, 1, 0x00020202);
	testJpegCsc("  Large stride", true, true, 0x00100002, 0x7FFFF, 0x00020202);
	// Hangs, maybe corrupts memory?
	//testJpegCsc("  Very large stride", true, true, 0x00100002, 0x7FFFFFFF, 0x00020202);
	testJpegCsc("  Very negative stride", true, true, 0x00100002, 0x80000000, 0x00020202);

	checkpointNext("Choma Sampling:");
	testJpegCsc("  Grayscale", true, true, 0x00100010, 0x0010, 0x00010101);
	testJpegCsc("  1x1", true, true, 0x00100010, 0x0010, 0x00020101);
	testJpegCsc("  2x1", true, true, 0x00100010, 0x0010, 0x00020201);
	testJpegCsc("  2x1", true, true, 0x00100010, 0x0010, 0x00020102);
	testJpegCsc("  3x3", true, true, 0x00100010, 0x0010, 0x00020303);
	testJpegCsc("  4x4", true, true, 0x00100010, 0x0010, 0x00020404);
	testJpegCsc("  Extra bits (top 12)", true, true, 0x00100010, 0x0010, 0xFFF20202);
	testJpegCsc("  Extra bits (top 13)", true, true, 0x00100010, 0x0010, 0xFFFA0202);

	checkpointNext("X/Y Pattern:");
	testJpegCscPattern("  2x2 short y", 0x00020202, 255, 64, 64);
	testJpegCscPattern("  2x2 short cb", 0x00020202, 256, 63, 64);
	testJpegCscPattern("  2x2 short cr", 0x00020202, 256, 64, 63);
	testJpegCscPattern("  2x1 short y", 0x00020201, 255, 128, 128);
	testJpegCscPattern("  2x1 short cb", 0x00020201, 256, 127, 128);
	testJpegCscPattern("  2x1 short cr", 0x00020201, 256, 128, 127);
	testJpegCscPattern("  1x1 short y", 0x00020101, 255, 256, 256);
	testJpegCscPattern("  1x1 short cb", 0x00020101, 256, 255, 256);
	testJpegCscPattern("  1x1 short cr", 0x00020101, 256, 256, 255);

	checkpointNext("Bits:");
	testJpegCscValue("  2x2 Black", 0x00020202, 0, 128, 128);
	testJpegCscValue("  2x2 White", 0x00020202, 255, 128, 128);
	testJpegCscValue("  2x2 0 Cb", 0x00020202, 255, 0, 128);
	testJpegCscValue("  2x2 1 Cb", 0x00020202, 255, 1, 128);
	testJpegCscValue("  2x2 254 Cb", 0x00020202, 255, 254, 128);
	testJpegCscValue("  2x2 255 Cb", 0x00020202, 255, 255, 128);
	testJpegCscValue("  2x2 0 Cr", 0x00020202, 255, 128, 0);
	testJpegCscValue("  2x2 1 Cr", 0x00020202, 255, 128, 1);
	testJpegCscValue("  2x2 254 Cr", 0x00020202, 255, 128, 254);
	testJpegCscValue("  2x2 255 Cr", 0x00020202, 255, 128, 255);

	testJpegCscValue("  2x1 Black", 0x00020201, 0, 128, 128);
	testJpegCscValue("  2x1 White", 0x00020201, 255, 128, 128);
	testJpegCscValue("  2x1 0 Cb", 0x00020201, 255, 0, 128);
	testJpegCscValue("  2x1 1 Cb", 0x00020201, 255, 1, 128);
	testJpegCscValue("  2x1 254 Cb", 0x00020201, 255, 254, 128);
	testJpegCscValue("  2x1 255 Cb", 0x00020201, 255, 255, 128);
	testJpegCscValue("  2x1 0 Cr", 0x00020201, 255, 128, 0);
	testJpegCscValue("  2x1 1 Cr", 0x00020201, 255, 128, 1);
	testJpegCscValue("  2x1 254 Cr", 0x00020201, 255, 128, 254);
	testJpegCscValue("  2x1 255 Cr", 0x00020201, 255, 128, 255);

	testJpegCscValue("  1x1 Black", 0x00020101, 0, 128, 128);
	testJpegCscValue("  1x1 White", 0x00020101, 255, 128, 128);
	testJpegCscValue("  1x1 0 Cb", 0x00020101, 255, 0, 128);
	testJpegCscValue("  1x1 1 Cb", 0x00020101, 255, 1, 128);
	testJpegCscValue("  1x1 254 Cb", 0x00020101, 255, 254, 128);
	testJpegCscValue("  1x1 255 Cb", 0x00020101, 255, 255, 128);
	testJpegCscValue("  1x1 0 Cr", 0x00020101, 255, 128, 0);
	testJpegCscValue("  1x1 1 Cr", 0x00020101, 255, 128, 1);
	testJpegCscValue("  1x1 254 Cr", 0x00020101, 255, 128, 254);
	testJpegCscValue("  1x1 255 Cr", 0x00020101, 255, 128, 255);

	free(frame);
	sceUtilityUnloadModule(PSP_MODULE_AV_AVCODEC);
	return 0;
}