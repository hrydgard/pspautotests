#include <common.h>
#include <pspjpeg.h>
#include <psputility.h>

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);

	checkpointNext("Basic:");
	checkpoint("  Before init: %08x", sceJpegDeleteMJpeg());
	checkpoint("  Init: %08x", sceJpegInitMJpeg());
	checkpoint("  After init: %08x", sceJpegDeleteMJpeg());
	checkpoint("  Create: %08x", sceJpegCreateMJpeg(480, 272));
	checkpoint("  After delete: %08x", sceJpegDeleteMJpeg());
	checkpoint("  Twice: %08x", sceJpegDeleteMJpeg());

	sceUtilityUnloadModule(PSP_MODULE_AV_AVCODEC);
	return 0;
}