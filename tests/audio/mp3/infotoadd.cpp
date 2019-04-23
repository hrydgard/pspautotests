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

static void testInfoToAdd(const char *title, int handle, u8 **dst, SceInt32 *towrite, SceInt32 *srcpos) {
	int result = sceMp3GetInfoToAddStreamData(handle, dst, towrite, srcpos);
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
	testInfoToAdd("  Unreserved", 0, &dst, &towrite, &srcpos);
	handle = sceMp3ReserveMp3Handle(NULL);
	testInfoToAdd("  NULL info arg", handle, &dst, &towrite, &srcpos);
	sceMp3ReleaseMp3Handle(handle);
	handle = sceMp3ReserveMp3Handle(&mp3Init);
	testInfoToAdd("  Basic info arg", handle, &dst, &towrite, &srcpos);
	sceMp3ReleaseMp3Handle(handle);
	testInfoToAdd("  Negative", -1, &dst, &towrite, &srcpos);
	testInfoToAdd("  2", 2, &dst, &towrite, &srcpos);

	//checkpoint("  Dst: enc+%08x, towrite: %08x, srcpos: %08x", dst - mp3Buf, towrite, srcpos);

	handle = sceMp3ReserveMp3Handle(&mp3Init);
	checkpointNext("Sequence");
	testInfoToAdd("  First", handle, &dst, &towrite, &srcpos);
	int result = sceMp3GetInfoToAddStreamData(handle, &dst2, &towrite2, &srcpos2);
	checkpoint("  Second: %08x - difference: dst=%d, towrite=%d, srcpos=%d", result, dst - dst2, towrite - towrite2, srcpos - srcpos2);
	int maxcopy = (int)sizeof(dummyMp3) / 2 > towrite2 ? towrite2 : (int)sizeof(dummyMp3) / 2;
	memcpy(dst2, dummyMp3, maxcopy);
	sceMp3NotifyAddStreamData(handle, maxcopy);
	result = sceMp3GetInfoToAddStreamData(handle, &dst2, &towrite2, &srcpos2);
	checkpoint("  Add half: %08x - difference: dst=%d, towrite=%d, srcpos=%d", result, dst2 - dst, towrite2 - towrite, srcpos2 - srcpos);
	maxcopy = (int)sizeof(dummyMp3) / 2 > towrite2 ? towrite2 : (int)sizeof(dummyMp3) / 2;
	if (srcpos2 + maxcopy <= (int)sizeof(dummyMp3)) {
		memcpy(dst2, dummyMp3 + srcpos2, maxcopy);
		sceMp3NotifyAddStreamData(handle, maxcopy);
		towrite2 = -1337;
		srcpos2 = -1337;
		result = sceMp3GetInfoToAddStreamData(handle, &dst2, &towrite2, &srcpos2);
		checkpoint("  Add remaining: %08x - now: dst=%08x, towrite=%d, srcpos=%d", result, dst2, towrite2, srcpos2);
	}
	sceMp3ReleaseMp3Handle(handle);

	handle = sceMp3ReserveMp3Handle(&mp3Init);
	checkpointNext("Post decode");
	sceMp3GetInfoToAddStreamData(handle, &dst2, &towrite2, &srcpos2);
	maxcopy = (int)sizeof(dummyMp3) > towrite2 ? towrite2 : (int)sizeof(dummyMp3);
	if (srcpos2 + maxcopy <= (int)sizeof(dummyMp3)) {
		memcpy(dst2, dummyMp3 + srcpos2, maxcopy);
	}
	sceMp3NotifyAddStreamData(handle, maxcopy);

	result = sceMp3GetInfoToAddStreamData(handle, &dst2, &towrite2, &srcpos2);
	checkpoint("  After add: %08x - now: dst=%08x, towrite=%d, srcpos=%d", result, dst2, towrite2, srcpos2);
	short *out = NULL;
	sceMp3Init(handle);
	sceMp3Decode(handle, &out);
	result = sceMp3GetInfoToAddStreamData(handle, &dst2, &towrite2, &srcpos2);
	checkpoint("  After decode: %08x - now: dst=%08x, towrite=%d, srcpos=%d", result, dst2, towrite2, srcpos2);

	handle = sceMp3ReserveMp3Handle(&mp3Init);
	checkpointNext("Pointers");
	testInfoToAdd("  All NULL", handle, NULL, NULL, NULL);
	testInfoToAdd("  Dest only", handle, &dst, NULL, NULL);
	testInfoToAdd("  To write only", handle, NULL, &towrite, NULL);
	testInfoToAdd("  Source pos only", handle, NULL, NULL, &srcpos);
	sceMp3ReleaseMp3Handle(handle);

	handle = sceMp3ReserveMp3Handle(&mp3Init);
	sceMp3TermResource();
	checkpointNext("After term");
	testInfoToAdd("  Prev allocated handle", handle, NULL, NULL, NULL);
}
