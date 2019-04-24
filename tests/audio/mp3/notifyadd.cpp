#include <common.h>
#include <pspiofilemgr.h>
#include <pspmp3.h>
#include <psputility.h>

static u8 mp3Buf[8192] __attribute__((aligned(64)));
static short pcmBuf[4608] __attribute__((aligned(64)));

static void testNotifyAdd(const char *title, int handle, int size) {
	int result = sceMp3NotifyAddStreamData(handle, size);
	SceInt32 towrite = 0;
	sceMp3GetInfoToAddStreamData(handle, NULL, &towrite, NULL);
	checkpoint("%s: %08x (%d to write)", title, result, towrite);
}

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_MP3);

	sceMp3InitResource();

	int fd = sceIoOpen("sample.mp3", PSP_O_RDONLY, 0777);

	u8 *dst = NULL;
	SceInt32 towrite = 0;
	SceInt32 srcpos = 0;
	int handle;

	SceMp3InitArg mp3Init;
	mp3Init.mp3StreamStart = 0;
	mp3Init.mp3StreamEnd = sceIoLseek32(fd, 0, SEEK_END);
	mp3Init.unk1 = 0;
	mp3Init.unk2 = 0;
	mp3Init.mp3Buf = mp3Buf;
	mp3Init.mp3BufSize = sizeof(mp3Buf);
	mp3Init.pcmBuf = pcmBuf;
	mp3Init.pcmBufSize = sizeof(pcmBuf);

	checkpointNext("Handles");
	testNotifyAdd("  Unreserved", 0, 0);
	handle = sceMp3ReserveMp3Handle(NULL);
	testNotifyAdd("  NULL info arg", handle, 0);
	sceMp3ReleaseMp3Handle(handle);
	handle = sceMp3ReserveMp3Handle(&mp3Init);
	testNotifyAdd("  Basic info arg", handle, 0);
	sceMp3ReleaseMp3Handle(handle);
	testNotifyAdd("  Negative", -1, 0);
	testNotifyAdd("  2", 2, 0);

	checkpointNext("Sizes");
	handle = sceMp3ReserveMp3Handle(&mp3Init);
	testNotifyAdd("  Size zero", handle, 0);
	testNotifyAdd("  Size negative", handle, -1);
	testNotifyAdd("  Size one", handle, 1);
	sceMp3ReleaseMp3Handle(handle);

	handle = sceMp3ReserveMp3Handle(&mp3Init);
	checkpointNext("Sequence");
	int chunks = 0;
	int stage = 0;
	while (sceMp3GetInfoToAddStreamData(handle, &dst, &towrite, &srcpos) == 0) {
		char temp[32];
		if (stage == 0) {
			snprintf(temp, sizeof(temp), "  Chunk %d", chunks + 1);
		} else if (stage == 1) {
			snprintf(temp, sizeof(temp), "  Chunk %d, after init", chunks + 1);
		} else if (stage == 2) {
			snprintf(temp, sizeof(temp), "  Chunk %d, after decode", chunks + 1);
		}
		int maxcopy = towrite;
		if (maxcopy > (int)mp3Init.mp3StreamEnd)
			maxcopy = (int)mp3Init.mp3StreamEnd;

		sceIoLseek32(fd, srcpos, SEEK_SET);
		sceIoRead(fd, dst, maxcopy);

		testNotifyAdd(temp, handle, maxcopy);
		if (chunks++ > 200 || towrite == 0) {
			if (stage == 0) {
				sceMp3Init(handle);
				stage = 1;
			} else if (stage == 1) {
				short *out = NULL;
				sceMp3Decode(handle, &out);
				stage = 2;
			} else {
				break;
			}
		}
	}
	sceMp3ReleaseMp3Handle(handle);

	handle = sceMp3ReserveMp3Handle(&mp3Init);
	sceMp3TermResource();
	checkpointNext("After term");
	testNotifyAdd("  Prev allocated handle", handle, 0);
}
