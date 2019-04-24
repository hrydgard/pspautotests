#include <common.h>
#include <pspiofilemgr.h>
#include <pspmp3.h>
#include <pspsysmem.h>
#include <psputility.h>
#include <vector>

static u8 mp3Buf[8192] __attribute__((aligned(64)));
static short pcmBuf[4608] __attribute__((aligned(64)));

static void testReleaseNew(const char *title, SceMp3InitArg *args) {
	int result = sceMp3ReserveMp3Handle(args);
	if (result >= 0) {
		result = sceMp3ReleaseMp3Handle(result);
		checkpoint("%s: %08x", title, result);
	} else {
		checkpoint("%s: Failed (%08x)", title, result);
	}
}

static void testRelease(const char *title, int handle) {
	int result = sceMp3ReleaseMp3Handle(handle);
	checkpoint("%s: %08x", title, result);
}

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_MP3);

	sceMp3InitResource();

	int fd = sceIoOpen("sample.mp3", PSP_O_RDONLY, 0777);

	SceMp3InitArg mp3Init;
	mp3Init.mp3StreamStart = 0;
	mp3Init.mp3StreamEnd = sceIoLseek32(fd, 0, SEEK_END);
	mp3Init.unk1 = 0;
	mp3Init.unk2 = 0;
	mp3Init.mp3Buf = mp3Buf;
	mp3Init.mp3BufSize = sizeof(mp3Buf);
	mp3Init.pcmBuf = pcmBuf;
	mp3Init.pcmBufSize = sizeof(pcmBuf);

	checkpointNext("Release handle types");
	testReleaseNew("  NULL", NULL);
	testReleaseNew("  Basic", &mp3Init);
	testRelease("  Double free", 0);
	testRelease("  Negative", -1);
	testRelease("  2", 2);

	int handle;
	SceInt32 srcpos;
	SceInt32 towrite;
	u8 *dst;
	checkpointNext("After init");
	handle = sceMp3ReserveMp3Handle(&mp3Init);
	sceMp3GetInfoToAddStreamData(handle, &dst, &towrite, &srcpos);
	sceIoLseek32(fd, srcpos, SEEK_SET);
	sceMp3NotifyAddStreamData(handle, sceIoRead(fd, dst, towrite));
	checkpoint("  Init: %08x", sceMp3Init(handle));
	testRelease("  After init", handle);

	handle = sceMp3ReserveMp3Handle(&mp3Init);
	sceMp3TermResource();
	checkpointNext("After term");
	testRelease("  Prev allocated handle", handle);
}