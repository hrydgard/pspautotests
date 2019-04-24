#include <common.h>
#include <pspmp3.h>
#include <psputility.h>

static u8 dummyMp3[72] __attribute__((aligned(64))) = {
	0xFF, 0xE3, 0x18, 0xC4,
	0x00, 0x00, 0x00, 0x03,
	0x48, 0x00, 0x00, 0x00,
	0x00, 0x4C, 0x41, 0x4D,
	0x45, 0x33, 0x2E, 0x39,
	0x38, 0x2E, 0x32, 0x00,
};
static u8 mp3Buf[8192] __attribute__((aligned(64)));
static short pcmBuf[4608] __attribute__((aligned(64)));

static void testCheckNeeded(const char *title, int handle) {
	int result = sceMp3CheckStreamDataNeeded(handle);
	checkpoint("%s: %08x", title, result);
}

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_MP3);

	sceMp3InitResource();

	u8 *dst = (u8 *)-1, *dst2 = NULL;
	SceInt32 towrite = 0x1337, towrite2 = 0;
	SceInt32 srcpos = 0x1337, srcpos2 = 0;
	int handle;

	SceMp3InitArg mp3Init;
	mp3Init.mp3StreamStart = 0;
	mp3Init.mp3StreamEnd = sizeof(dummyMp3);
	mp3Init.unk1 = 0;
	mp3Init.unk2 = 0;
	mp3Init.mp3Buf = mp3Buf;
	mp3Init.mp3BufSize = sizeof(mp3Buf);
	mp3Init.pcmBuf = pcmBuf;
	mp3Init.pcmBufSize = sizeof(pcmBuf);

	checkpointNext("Handles");
	testCheckNeeded("  Unreserved", 0);
	handle = sceMp3ReserveMp3Handle(NULL);
	testCheckNeeded("  NULL info arg", handle);
	sceMp3ReleaseMp3Handle(handle);
	handle = sceMp3ReserveMp3Handle(&mp3Init);
	testCheckNeeded("  Basic info arg", handle);
	sceMp3ReleaseMp3Handle(handle);
	testCheckNeeded("  Negative", -1);
	testCheckNeeded("  2", 2);

	handle = sceMp3ReserveMp3Handle(&mp3Init);
	checkpointNext("Sequence");
	testCheckNeeded("  First", handle);
	testCheckNeeded("  Second", handle);
	sceMp3GetInfoToAddStreamData(handle, &dst, &towrite, &srcpos);
	int maxcopy = (int)sizeof(dummyMp3) / 2 > towrite ? towrite : (int)sizeof(dummyMp3) / 2;
	memcpy(dst, dummyMp3, maxcopy);
	sceMp3NotifyAddStreamData(handle, maxcopy);
	testCheckNeeded("  Add half", handle);
	sceMp3GetInfoToAddStreamData(handle, &dst, &towrite, &srcpos);
	maxcopy = (int)sizeof(dummyMp3) / 2 > towrite ? towrite : (int)sizeof(dummyMp3) / 2;
	if (srcpos + maxcopy <= (int)sizeof(dummyMp3)) {
		memcpy(dst, dummyMp3 + srcpos, maxcopy);
		sceMp3NotifyAddStreamData(handle, maxcopy);
		testCheckNeeded("  Add remaining", handle);
	}
	sceMp3ReleaseMp3Handle(handle);

	handle = sceMp3ReserveMp3Handle(&mp3Init);
	checkpointNext("Post decode");
	sceMp3GetInfoToAddStreamData(handle, &dst, &towrite, &srcpos);
	maxcopy = (int)sizeof(dummyMp3) > towrite ? towrite : (int)sizeof(dummyMp3);
	if (srcpos + maxcopy <= (int)sizeof(dummyMp3)) {
		memcpy(dst, dummyMp3 + srcpos, maxcopy);
	}
	sceMp3NotifyAddStreamData(handle, maxcopy);

	testCheckNeeded("  After add", handle);
	short *out = NULL;
	sceMp3Init(handle);
	sceMp3Decode(handle, &out);
	testCheckNeeded("  After decode", handle);

	handle = sceMp3ReserveMp3Handle(&mp3Init);
	sceMp3TermResource();
	checkpointNext("After term");
	testCheckNeeded("  Prev allocated handle", handle);
}
