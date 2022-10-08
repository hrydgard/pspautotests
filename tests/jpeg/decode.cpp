#include "shared.h"
#include <malloc.h>
#include <psputility.h>
#include <psputils.h>

static uint32_t *frame;
static const uint32_t FRAME_SIZE = 1024 * 512 * 4;

static void testJpegDecodeMJpeg(const char *title, const File &file, bool useFrame, int mode) {
	memset(frame, 0xCC, FRAME_SIZE + 4);
	sceKernelDcacheWritebackInvalidateRange(frame, FRAME_SIZE + 4);

	uint32_t result = sceJpegDecodeMJpeg(file.data, file.size, useFrame ? frame : NULL, mode);
	size_t len = 0;
	for (int i = 512 * 512; i >= 0; --i) {
		if (frame[i] != 0xCCCCCCCC) {
			len = (i + 1) * sizeof(uint32_t);
			break;
		}
	}
	size_t rowLen = 0;
	for (int i = 0; i < len; ++i) {
		if (frame[i] == 0xCCCCCCCC) {
			rowLen = i * sizeof(uint32_t);
			break;
		}
	}
	checkpoint("%s: %08x (wrote %d bytes, %d per row)", title, result, len, rowLen);
}

static void testJpegDecodeMJpegSize(const char *title, const File &file, bool useFrame, int mode, int w, int h) {
	sceJpegDeleteMJpeg();
	sceJpegCreateMJpeg(w, h);

	testJpegDecodeMJpeg(title, file, useFrame, mode);

	sceJpegDeleteMJpeg();
	sceJpegCreateMJpeg(512, 272);
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
	testJpegDecodeMJpeg("  Before init", jpegcolor, true, 1);
	checkpoint("  Init: %08x", sceJpegInitMJpeg());
	testJpegDecodeMJpeg("  After init", jpegcolor, true, 1);
	checkpoint("  Create: %08x", sceJpegCreateMJpeg(512, 272));
	testJpegDecodeMJpeg("  After create", jpegcolor, true, 1);
	checkpoint("  Delete: %08x", sceJpegDeleteMJpeg());
	testJpegDecodeMJpeg("  After delete", jpegcolor, true, 1);
	checkpoint("  Finish: %08x", sceJpegFinishMJpeg());
	testJpegDecodeMJpeg("  After finish", jpegcolor, true, 1);
	sceJpegInitMJpeg();
	sceJpegCreateMJpeg(512, 272);

	checkpointNext("Data:");
	testJpegDecodeMJpeg("  NULL", invalid, true, 0);
	testJpegDecodeMJpeg("  Empty", empty, true, 0);
	testJpegDecodeMJpeg("  Size 0", File(jpegcolor, 0), true, 0);
	testJpegDecodeMJpeg("  Size 1", File(jpegcolor, 1), true, 0);
	testJpegDecodeMJpeg("  Huge", File(jpegcolor, 0x7FFFFFFF), true, 0);
	testJpegDecodeMJpeg("  Negative", File(jpegcolor, 0x80000000), true, 0);
	checkpoint("  Offset: %08x", sceJpegDecodeMJpeg(jpegcolor.data - 1, jpegcolor.size + 1, frame, 0));

	checkpointNext("Output:");
	testJpegDecodeMJpeg("  Output NULL", jpegcolor, false, 0);
	testJpegDecodeMJpeg("  Output NULL again", jpegcolor, false, 0);
	testJpegDecodeMJpegSize("  Frame 512x16", jpegcolor, true, 0, 512, 16);
	testJpegDecodeMJpegSize("  Frame 480x271", jpegcolor, true, 0, 480, 271);
	testJpegDecodeMJpegSize("  Frame 480x272", jpegcolor, true, 0, 480, 272);
	testJpegDecodeMJpegSize("  Frame 480x273", jpegcolor, true, 0, 480, 273);
	testJpegDecodeMJpegSize("  Frame 479x272", jpegcolor, true, 0, 479, 272);
	testJpegDecodeMJpegSize("  Frame 1024x271", jpegcolor, true, 0, 1024, 271);
	testJpegDecodeMJpegSize("  Frame 1024x272", jpegcolor, true, 0, 1024, 272);
	testJpegDecodeMJpegSize("  Frame 0x0", jpegcolor, true, 0, 0, 0);
	testJpegDecodeMJpegSize("  Frame -1x-1", jpegcolor, true, 0, -1, -1);

	checkpointNext("Mode:");
	testJpegDecodeMJpeg("  Mode 1", jpegcolor, true, 1);
	testJpegDecodeMJpeg("  Mode 2", jpegcolor, true, 2);
	testJpegDecodeMJpeg("  Mode 3", jpegcolor, true, 3);
	testJpegDecodeMJpeg("  Mode 999", jpegcolor, true, 999);
	testJpegDecodeMJpeg("  Mode -1", jpegcolor, true, -1);

	checkpointNext("Colors:");
	testJpegDecodeMJpeg("  Standard 2x2", jpegcolor, true, 1);
	testJpegDecodeMJpeg("  Color 2x1", jpeg2x1, true, 1);
	testJpegDecodeMJpeg("  Grayscale", jpeggray, true, 1);
	testJpegDecodeMJpeg("  Black", jpeggeb, true, 1);

	free(frame);
	sceUtilityUnloadModule(PSP_MODULE_AV_AVCODEC);
	return 0;
}