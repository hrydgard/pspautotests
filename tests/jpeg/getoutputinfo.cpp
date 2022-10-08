#include "shared.h"
#include <psputility.h>

extern "C" int sceJpegGetOutputInfo(const void *buffer, int bufferSize, u32 *info, int dhtMode);

static void testJpegGetOutputInfo(const char *title, const File &file, bool useInfo, int mode) {
	uint32_t info = 0xCCCCCCCC;
	uint32_t result = sceJpegGetOutputInfo(file.data, file.size, useInfo ? &info : NULL, mode);
	checkpoint("%s: %08x (%08x)", title, result, useInfo ? info : 0xFFFFFFFF);
}

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);

	File empty;
	File invalid(true, 0);
	File jpegcolor("background.jpg");
	File jpeggray("backgroundgray.jpg");
	File jpeg2x1("background2x1.jpg");
	if (!empty.data || !jpegcolor.data || !jpeggray.data || !jpeg2x1.data) {
		return 1;
	}

	uint32_t info;

	checkpointNext("Basic:");
	// Apparently doesn't need init.
	testJpegGetOutputInfo("  Before init", jpegcolor, true, 0);
	checkpoint("  Init: %08x", sceJpegInitMJpeg());
	testJpegGetOutputInfo("  After init", jpegcolor, true, 0);

	checkpointNext("Data:");
	testJpegGetOutputInfo("  NULL", invalid, true, 0);
	testJpegGetOutputInfo("  Empty", empty, true, 0);
	testJpegGetOutputInfo("  Size 0", File(jpegcolor, 0), true, 0);
	testJpegGetOutputInfo("  Size 1", File(jpegcolor, 1), true, 0);
	testJpegGetOutputInfo("  Huge", File(jpegcolor, 0x7FFFFFFF), true, 0);
	testJpegGetOutputInfo("  Negative", File(jpegcolor, 0x80000000), true, 0);
	checkpoint("  Offset: %08x", sceJpegGetOutputInfo(jpegcolor.data - 1, jpegcolor.size + 1, &info, 0));

	checkpointNext("Info:");
	testJpegGetOutputInfo("  NULL info", jpegcolor, false, 0);

	checkpointNext("Mode:");
	testJpegGetOutputInfo("  Mode 1", jpegcolor, true, 1);
	testJpegGetOutputInfo("  Mode 2", jpegcolor, true, 2);
	testJpegGetOutputInfo("  Mode 3", jpegcolor, true, 3);
	testJpegGetOutputInfo("  Mode 999", jpegcolor, true, 999);
	testJpegGetOutputInfo("  Mode -1", jpegcolor, true, -1);

	checkpointNext("Colors:");
	testJpegGetOutputInfo("  Standard 2x2", jpegcolor, true, 1);
	testJpegGetOutputInfo("  Color 2x1", jpeg2x1, true, 1);
	testJpegGetOutputInfo("  Grayscale", jpeggray, true, 1);

	sceUtilityUnloadModule(PSP_MODULE_AV_AVCODEC);
	return 0;
}