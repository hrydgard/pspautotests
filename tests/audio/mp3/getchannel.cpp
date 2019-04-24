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

static void testGetChannelNum(const char *title, int handle) {
	int result = sceMp3GetMp3ChannelNum(handle);
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
	testGetChannelNum("  Unreserved", 0);
	handle = sceMp3ReserveMp3Handle(NULL);
	testGetChannelNum("  NULL info arg", handle);
	sceMp3ReleaseMp3Handle(handle);
	handle = sceMp3ReserveMp3Handle(&mp3Init);
	testGetChannelNum("  Basic info arg", handle);
	sceMp3ReleaseMp3Handle(handle);
	testGetChannelNum("  Negative", -1);
	testGetChannelNum("  2", 2);

	static const u8 baseHeader[] = {
		0xFF, 0xFB, 0x10, 0x00,
	};

	checkpointNext("After init");
	handle = sceMp3ReserveMp3Handle(NULL);
	sceMp3LowLevelInit(handle, 0);
	testGetChannelNum("  Low level init", handle);
	sceIoLseek32(fd, 0, SEEK_SET);
	sceIoRead(fd, mp3Buf, 4096);
	int consumed, written;
	sceMp3LowLevelDecode(handle, mp3Buf, &consumed, pcmBuf, &written);
	testGetChannelNum("  Low level decode", handle);
	sceMp3ReleaseMp3Handle(handle);
	handle = initHeader(&mp3Init, baseHeader, sizeof(baseHeader));
	testGetChannelNum("  After init", handle);
	sceMp3ReleaseMp3Handle(handle);
	handle = initHeader(&mp3Init, baseHeader, sizeof(baseHeader));
	sceMp3Decode(handle, &out);
	testGetChannelNum("  After decode", handle);
	sceMp3ReleaseMp3Handle(handle);

	checkpointNext("Channels");
	static const char *channels[] = { "stereo", "joint stereo", "dual channel", "mono" };
	for (int bits = 0; bits < ARRAY_SIZE(channels); ++bits) {
		char temp[64];
		snprintf(temp, sizeof(temp), "  Channel %s", channels[bits]);
		u8 header[4];
		memcpy(header, baseHeader, sizeof(header));
		header[3] = bits << 6;
		handle = initHeader(&mp3Init, header, sizeof(header));
		if (handle < 0) {
			checkpoint("%s: Failed (%08x)", temp, handle);
		} else {
			testGetChannelNum(temp, handle);
		}
		sceMp3ReleaseMp3Handle(handle);
	}

	handle = sceMp3ReserveMp3Handle(&mp3Init);
	sceMp3TermResource();
	checkpointNext("After term");
	testGetChannelNum("  Prev allocated handle", handle);

	return 0;
}
