#include <common.h>
#include <pspiofilemgr.h>
#include <pspmp3.h>
#include <psputility.h>
#include <psputils.h>

extern "C" int sceMp3LowLevelInit(int handle, int unk);
extern "C" int sceMp3LowLevelDecode(int handle, const void *src, int *srcConsumed, short *samples, int *sampleBytesWritten);

static u8 mp3Buf[8192] __attribute__((aligned(64)));
static short pcmBuf[4608] __attribute__((aligned(64)));

#define CHECK_ERROR(a) checkError(#a, a);

static int checkError(const char *call, int result) {
	if (result < 0) {
		schedf("ERROR: Unexpected error %08x from %s\n", result, call);
	}
}

static void testGetSumDecoded(const char *title, int handle) {
	int result = sceMp3GetSumDecodedSample(handle);
	if (result >= 0) {
		checkpoint("%s: %d", title, result);
	} else {
		checkpoint("%s: %08x", title, result);
	}
}

static void checkSumLoopChange(const char *title, int handle, int &sum, int &loop) {
	int newSum = sceMp3GetSumDecodedSample(handle);
	int newLoop = sceMp3GetLoopNum(handle);

	if (newSum < sum && newLoop != loop) {
		checkpoint("%s: loop at %d, sum %d => %d", title, newLoop, sum, newSum);
	} else if (newSum < sum) {
		checkpoint("%s: sum at %d => %d", title, sum, newSum);
	} else if (newLoop != loop) {
		checkpoint("%s: loop at %d", title, newLoop);
	}

	sum = newSum;
	loop = newLoop;
}

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_MP3);

	sceMp3InitResource();

	int fd = sceIoOpen("sample.mp3", PSP_O_RDONLY, 0777);

	u8 *dst;
	SceInt32 towrite, srcpos;
	short *out = NULL;
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
	testGetSumDecoded("  Unreserved", 0);
	handle = sceMp3ReserveMp3Handle(NULL);
	testGetSumDecoded("  NULL info arg", handle);
	sceMp3ReleaseMp3Handle(handle);
	handle = sceMp3ReserveMp3Handle(&mp3Init);
	testGetSumDecoded("  Basic info arg", handle);
	sceMp3ReleaseMp3Handle(handle);
	testGetSumDecoded("  Negative", -1);
	testGetSumDecoded("  2", 2);

	checkpointNext("After decoding");
	handle = sceMp3ReserveMp3Handle(NULL);
	sceMp3LowLevelInit(handle, 0);
	testGetSumDecoded("  Low level init", handle);
	sceIoLseek32(fd, 0, SEEK_SET);
	sceIoRead(fd, mp3Buf, 4096);
	int consumed, written;
	sceMp3LowLevelDecode(handle, mp3Buf, &consumed, pcmBuf, &written);
	testGetSumDecoded("  Low level decode", handle);
	sceMp3ReleaseMp3Handle(handle);
	handle = sceMp3ReserveMp3Handle(&mp3Init);
	if (sceMp3GetInfoToAddStreamData(handle, &dst, &towrite, &srcpos) >= 0) {
		sceIoLseek32(fd, srcpos, SEEK_SET);
		sceMp3NotifyAddStreamData(handle, sceIoRead(fd, dst, towrite));
		sceMp3Init(handle);
		testGetSumDecoded("  After init", handle);
		sceMp3Decode(handle, &out);
		testGetSumDecoded("  After decode 1", handle);
		sceMp3Decode(handle, &out);
		testGetSumDecoded("  After decode 2", handle);
	}
	sceMp3ReleaseMp3Handle(handle);

	checkpointNext("Channels");
	static const char *channels[] = { "stereo", "joint stereo", "dual channel", "mono" };
	for (u32 bits = 0; bits < ARRAY_SIZE(channels); ++bits) {
		char temp[64];
		snprintf(temp, sizeof(temp), "  Channel %s", channels[bits]);

		handle = sceMp3ReserveMp3Handle(&mp3Init);
		sceMp3GetInfoToAddStreamData(handle, &dst, &towrite, &srcpos);
		sceIoLseek32(fd, srcpos, SEEK_SET);
		sceMp3NotifyAddStreamData(handle, sceIoRead(fd, dst, towrite));

		// Overwrite the channel bits.
		dst[3] = (dst[3] & ~(3 << 6)) | bits << 6;

		sceMp3Init(handle);
		sceMp3Decode(handle, &out);
		testGetSumDecoded(temp, handle);
		sceMp3ReleaseMp3Handle(handle);
	}

	checkpointNext("Loop");
	handle = sceMp3ReserveMp3Handle(&mp3Init);
	sceMp3SetLoopNum(handle, 2);
	int lastSum = 0, lastLoop = 2;
	for (int step = 0; step < 5000; ++step) {
		CHECK_ERROR(sceMp3GetInfoToAddStreamData(handle, &dst, &towrite, &srcpos));
		checkSumLoopChange("  After get add info", handle, lastSum, lastLoop);
		if (towrite != 0) {
			sceIoLseek32(fd, srcpos, SEEK_SET);
			CHECK_ERROR(sceMp3NotifyAddStreamData(handle, sceIoRead(fd, dst, towrite)));
			checkSumLoopChange("  After add", handle, lastSum, lastLoop);
		}
		if (step == 0) {
			CHECK_ERROR(sceMp3Init(handle));
		}
		int decoded = CHECK_ERROR(sceMp3Decode(handle, &out));
		if (decoded <= 0) {
			checkSumLoopChange("  After decode finish", handle, lastSum, lastLoop);
			break;
		} else {
			checkSumLoopChange("  After decode", handle, lastSum, lastLoop);
		}
		if (step == 4999) {
			checkpoint("ERROR: Should have looped by now");
		}
	}
	testGetSumDecoded("  End of loops", handle);

	checkpointNext("Start pos");
	mp3Init.mp3StreamStart = 1;
	handle = sceMp3ReserveMp3Handle(&mp3Init);
	sceMp3SetLoopNum(handle, 2);
	lastSum = 0;
	lastLoop = 2;
	for (int step = 0; step < 5000; ++step) {
		CHECK_ERROR(sceMp3GetInfoToAddStreamData(handle, &dst, &towrite, &srcpos));
		checkSumLoopChange("  After get add info", handle, lastSum, lastLoop);
		if (towrite != 0) {
			sceIoLseek32(fd, srcpos, SEEK_SET);
			CHECK_ERROR(sceMp3NotifyAddStreamData(handle, sceIoRead(fd, dst, towrite)));
			checkSumLoopChange("  After add", handle, lastSum, lastLoop);
		}
		if (step == 0) {
			CHECK_ERROR(sceMp3Init(handle));
		}
		int decoded = CHECK_ERROR(sceMp3Decode(handle, &out));
		if (decoded <= 0) {
			checkSumLoopChange("  After decode finish", handle, lastSum, lastLoop);
			break;
		} else {
			checkSumLoopChange("  After decode", handle, lastSum, lastLoop);
		}
		if (step == 4999) {
			checkpoint("ERROR: Should have looped by now");
		}
	}
	testGetSumDecoded("  End of offset start", handle);

	handle = sceMp3ReserveMp3Handle(&mp3Init);
	sceMp3TermResource();
	checkpointNext("After term");
	testGetSumDecoded("  Prev allocated handle", handle);

	return 0;
}
