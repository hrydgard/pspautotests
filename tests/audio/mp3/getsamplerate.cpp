#include <common.h>
#include <pspiofilemgr.h>
#include <pspmp3.h>
#include <psputility.h>
#include <psputils.h>

extern "C" int sceMp3LowLevelInit(int handle, int unk);
extern "C" int sceMp3LowLevelDecode(int handle, const void *src, int *srcConsumed, short *samples, int *sampleBytesWritten);

static u8 mp3Buf[8192] __attribute__((aligned(64)));
static short pcmBuf[4608] __attribute__((aligned(64)));

static int initHeader(SceMp3InitArg *mp3Init, const u8 *header, int size, int offset = 0) {
	memset(mp3Buf, 0, sizeof(mp3Buf));
	int handle = sceMp3ReserveMp3Handle(mp3Init);
	if (handle < 0) {
		return handle;
	}

	u8 *dst = NULL;
	int result = sceMp3GetInfoToAddStreamData(handle, &dst, NULL, NULL);
	if (result < 0) {
		sceMp3ReleaseMp3Handle(handle);
		return result;
	}

	memcpy(dst + offset, header, size);
	sceKernelDcacheWritebackInvalidateRange(mp3Buf, sizeof(mp3Buf));

	result = sceMp3Init(handle);
	if (result < 0) {
		sceMp3ReleaseMp3Handle(handle);
		return result;
	}
	return handle;
}

static void testGetSamplingRate(const char *title, int handle) {
	int result = sceMp3GetSamplingRate(handle);
	if (result >= 0) {
		checkpoint("%s: %d", title, result);
	} else {
		checkpoint("%s: %08x", title, result);
	}
}

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_MP3);

	sceMp3InitResource();

	int fd = sceIoOpen("sample.mp3", PSP_O_RDONLY, 0777);

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
	testGetSamplingRate("  Unreserved", 0);
	handle = sceMp3ReserveMp3Handle(NULL);
	testGetSamplingRate("  NULL info arg", handle);
	sceMp3ReleaseMp3Handle(handle);
	handle = sceMp3ReserveMp3Handle(&mp3Init);
	testGetSamplingRate("  Basic info arg", handle);
	sceMp3ReleaseMp3Handle(handle);
	testGetSamplingRate("  Negative", -1);
	testGetSamplingRate("  2", 2);

	static const u8 header44_1[] = {
		0xFF, 0xFB, 0x10, 0x00,
	};
	static const u8 header48[] = {
		0xFF, 0xFB, 0x14, 0x00,
	};
	static const u8 header32[] = {
		0xFF, 0xFB, 0x18, 0x00,
	};

	checkpointNext("After init");
	handle = sceMp3ReserveMp3Handle(NULL);
	sceMp3LowLevelInit(handle, 0);
	testGetSamplingRate("  Low level init", handle);
	sceIoLseek32(fd, 0, SEEK_SET);
	sceIoRead(fd, mp3Buf, 4096);
	int consumed, written;
	sceMp3LowLevelDecode(handle, mp3Buf, &consumed, pcmBuf, &written);
	testGetSamplingRate("  Low level decode", handle);
	sceMp3ReleaseMp3Handle(handle);
	handle = initHeader(&mp3Init, header44_1, sizeof(header44_1));
	testGetSamplingRate("  44.1kHz", handle);
	sceMp3ReleaseMp3Handle(handle);
	handle = initHeader(&mp3Init, header44_1, sizeof(header44_1));
	sceMp3Decode(handle, &out);
	testGetSamplingRate("  After decode", handle);
	sceMp3ReleaseMp3Handle(handle);
	handle = initHeader(&mp3Init, header48, sizeof(header48));
	testGetSamplingRate("  48kHz", handle);
	sceMp3ReleaseMp3Handle(handle);
	handle = initHeader(&mp3Init, header32, sizeof(header32));
	testGetSamplingRate("  32kHz", handle);
	sceMp3ReleaseMp3Handle(handle);

	handle = sceMp3ReserveMp3Handle(&mp3Init);
	sceMp3TermResource();
	checkpointNext("After term");
	testGetSamplingRate("  Prev allocated handle", handle);

	return 0;
}
