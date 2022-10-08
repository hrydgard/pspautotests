#include <common.h>
#include <pspjpeg.h>
#include <psputility.h>

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);

	checkpointNext("Basic:");
	checkpoint("  Before init: %08x", sceJpegCreateMJpeg(480, 272));
	checkpoint("  Init: %08x", sceJpegInitMJpeg());
	checkpoint("  After init: %08x", sceJpegCreateMJpeg(480, 272));
	checkpoint("  Twice: %08x", sceJpegCreateMJpeg(480, 272));
	checkpoint("  Delete: %08x", sceJpegDeleteMJpeg());
	checkpoint("  After delete: %08x", sceJpegCreateMJpeg(480, 272));
	sceJpegDeleteMJpeg();

	checkpointNext("Widths:");
	static const int sizes[] = { 0x80000000, 0xFFFF0000, 0xFFFF8000, -1, 0, 1, 2, 3, 4, 5, 480, 512, 1023, 1024, 1025, 2048, 0x8000, 0x7FFFFFFF};
	for (size_t i = 0; i < ARRAY_SIZE(sizes); ++i) {
		char temp[256];
		checkpoint("  Width %d: %08x", sizes[i], sceJpegCreateMJpeg(sizes[i], 272));
		sceJpegDeleteMJpeg();
	}

	checkpointNext("Heights:");
	for (size_t i = 0; i < ARRAY_SIZE(sizes); ++i) {
		char temp[256];
		checkpoint("  Height %d: %08x", sizes[i], sceJpegCreateMJpeg(480, sizes[i]));
		sceJpegDeleteMJpeg();
	}

	checkpointNext("Total:");
	checkpoint("  Size 1024x1024: %08x", sceJpegCreateMJpeg(1024, 1024));
	sceJpegDeleteMJpeg();

	sceUtilityUnloadModule(PSP_MODULE_AV_AVCODEC);
	return 0;
}