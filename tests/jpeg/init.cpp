#include <common.h>
#include <pspjpeg.h>
#include <psputility.h>

extern "C" int main(int argc, char *argv[]) {
	checkpointNext("Modules:");
	checkpoint("  LoadAv: %08x", sceUtilityLoadAvModule(PSP_AV_MODULE_AVCODEC));
	checkpoint("  UnloadAv: %08x", sceUtilityUnloadAvModule(PSP_AV_MODULE_AVCODEC));
	checkpoint("  Load: %08x", sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC));

	checkpointNext("Sequence:");
	checkpoint("  Init: %08x", sceJpegInitMJpeg());
	checkpoint("  Twice: %08x", sceJpegInitMJpeg());
	checkpoint("  Finish: %08x", sceJpegFinishMJpeg());
	checkpoint("  After finish: %08x", sceJpegInitMJpeg());

	return 0;
}