#include "shared.h"
#include <malloc.h>
#include <psputility.h>
#include <psputils.h>

extern "C" int sceJpegDecodeMJpegYCbCr(const void *buffer, int bufferSize, void *frame, int frameSize, int dhtMode);

static uint32_t *frame;
static const uint32_t FRAME_SIZE = 512 * 512 * 4;

static void testJpegDecodeMJpegYCbCr(const char *title, const File &file, bool useFrame, int frameSize, int mode) {
	memset(frame, 0xCC, FRAME_SIZE + 4);
	sceKernelDcacheWritebackInvalidateRange(frame, FRAME_SIZE + 4);

	uint32_t result = sceJpegDecodeMJpegYCbCr(file.data, file.size, useFrame ? frame : NULL, frameSize, mode);
	size_t len = 0;
	for (int i = 512 * 512; i >= 0; --i) {
		if (frame[i] != 0xCCCCCCCC) {
			len = (i + 1) * sizeof(uint32_t);
			break;
		}
	}
	checkpoint("%s: %08x (wrote %d bytes)", title, result, len);
}

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);

	File empty;
	File invalid(true, 0);
	File jpegcolor("background.jpg");
	File jpeggray("backgroundgray.jpg");
	File jpeg2x1("background2x1.jpg");
	File jpeggeb("geb.jpg");
	if (!empty.data || !jpegcolor.data || !jpeggray.data || !jpeg2x1.data || !jpeggeb.data) {
		return 1;
	}

	frame = (uint32_t *)malloc(FRAME_SIZE + 4);

	checkpointNext("Basic:");
	testJpegDecodeMJpegYCbCr("  Before init", jpegcolor, true, FRAME_SIZE, 1);
	checkpoint("  Init: %08x", sceJpegInitMJpeg());
	testJpegDecodeMJpegYCbCr("  After init", jpegcolor, true, FRAME_SIZE, 1);
	checkpoint("  Create: %08x", sceJpegCreateMJpeg(512, 272));
	testJpegDecodeMJpegYCbCr("  After create", jpegcolor, true, FRAME_SIZE, 1);

	checkpointNext("Data:");
	testJpegDecodeMJpegYCbCr("  NULL", invalid, true, FRAME_SIZE, 0);
	testJpegDecodeMJpegYCbCr("  Empty", empty, true, FRAME_SIZE, 0);
	testJpegDecodeMJpegYCbCr("  Size 0", File(jpegcolor, 0), true, FRAME_SIZE, 0);
	testJpegDecodeMJpegYCbCr("  Size 1", File(jpegcolor, 1), true, FRAME_SIZE, 0);
	testJpegDecodeMJpegYCbCr("  Huge", File(jpegcolor, 0x7FFFFFFF), true, FRAME_SIZE, 0);
	testJpegDecodeMJpegYCbCr("  Negative", File(jpegcolor, 0x80000000), true, FRAME_SIZE, 0);
	checkpoint("  Offset: %08x", sceJpegDecodeMJpegYCbCr(jpegcolor.data - 1, jpegcolor.size + 1, frame, FRAME_SIZE, 0));

	checkpointNext("Output:");
	testJpegDecodeMJpegYCbCr("  Output NULL small", jpegcolor, false, 195839, 0);
	testJpegDecodeMJpegYCbCr("  Output NULL", jpegcolor, false, FRAME_SIZE, 0);
	testJpegDecodeMJpegYCbCr("  Output NULL again", jpegcolor, false, FRAME_SIZE, 0);
	testJpegDecodeMJpegYCbCr("  Output valid, size negative", jpegcolor, true, -1, 0);
	testJpegDecodeMJpegYCbCr("  Output valid, small aligned", jpegcolor, true, 195824, 0);
	testJpegDecodeMJpegYCbCr("  Output valid, small", jpegcolor, true, 195839, 0);
	testJpegDecodeMJpegYCbCr("  Output valid, exact", jpegcolor, true, 195840, 0);

	checkpointNext("Mode:");
	testJpegDecodeMJpegYCbCr("  Mode 1", jpegcolor, true, FRAME_SIZE, 1);
	testJpegDecodeMJpegYCbCr("  Mode 2", jpegcolor, true, FRAME_SIZE, 2);
	testJpegDecodeMJpegYCbCr("  Mode 3", jpegcolor, true, FRAME_SIZE, 3);
	testJpegDecodeMJpegYCbCr("  Mode 999", jpegcolor, true, FRAME_SIZE, 999);
	testJpegDecodeMJpegYCbCr("  Mode -1", jpegcolor, true, FRAME_SIZE, -1);

	checkpointNext("Colors:");
	testJpegDecodeMJpegYCbCr("  Standard 2x2", jpegcolor, true, FRAME_SIZE, 1);
	testJpegDecodeMJpegYCbCr("  Color 2x1", jpeg2x1, true, FRAME_SIZE, 1);
	testJpegDecodeMJpegYCbCr("  Grayscale", jpeggray, true, FRAME_SIZE, 1);
	testJpegDecodeMJpegYCbCr("  Black", jpeggeb, true, FRAME_SIZE, 1);

	free(frame);
	sceUtilityUnloadModule(PSP_MODULE_AV_AVCODEC);
	return 0;
}