#include <common.h>
#include <pspmp3.h>
#include <pspsysmem.h>
#include <psputility.h>

static void testInit(const char *title) {
	SceSize before = sceKernelTotalFreeMemSize();
	int result = sceMp3InitResource();
	SceSize after = sceKernelTotalFreeMemSize();
	checkpoint("%s: %08x, used %d bytes", title, result, after - before);
}

static void testTerm(const char *title) {
	SceSize before = sceKernelTotalFreeMemSize();
	int result = sceMp3TermResource();
	SceSize after = sceKernelTotalFreeMemSize();
	checkpoint("%s: %08x, used %d bytes", title, result, after - before);
}

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_MP3);

	checkpointNext("sceMp3InitResource:");
	testInit("  Once");
	testInit("  Twice");

	int handle = sceMp3ReserveMp3Handle(NULL);
	checkpoint("  Reserve after init: %08x", handle);
	sceMp3ReleaseMp3Handle(handle);

	checkpointNext("sceMp3TermResource:");
	testTerm("  Once");
	testTerm("  Twice");
	testTerm("  Thrice");

	checkpoint("  Reserve after term: %08x", sceMp3ReserveMp3Handle(NULL));
}