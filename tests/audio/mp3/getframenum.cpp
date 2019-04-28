#include <common.h>
#include <pspiofilemgr.h>
#include <pspmp3.h>
#include <psputility.h>
#include <psputils.h>

extern "C" int sceMp3LowLevelInit(int handle, int unk);
extern "C" int sceMp3LowLevelDecode(int handle, const void *src, int *srcConsumed, short *samples, int *sampleBytesWritten);
extern "C" int sceMp3GetFrameNum(int handle);

static u8 mp3Buf[8192] __attribute__((aligned(64)));
static short pcmBuf[4608] __attribute__((aligned(64)));

static int initHeader(int handle, const u8 *header, int size, int offset = 0) {
	memset(mp3Buf, 0, sizeof(mp3Buf));

	u8 *dst = NULL;
	int result = sceMp3GetInfoToAddStreamData(handle, &dst, NULL, NULL);
	if (result < 0) {
		sceMp3ReleaseMp3Handle(handle);
		return result;
	}

	memcpy(dst + offset, header, size);
	sceKernelDcacheWritebackInvalidateRange(mp3Buf, sizeof(mp3Buf));

	return sceMp3Init(handle);
}

static int initHeader(SceMp3InitArg *mp3Init, const u8 *header, int size, int offset = 0) {
	int handle = sceMp3ReserveMp3Handle(mp3Init);
	if (handle < 0) {
		return handle;
	}

	int result = initHeader(handle, header, size, offset);
	if (result < 0) {
		sceMp3ReleaseMp3Handle(handle);
		return result;
	}
	return handle;
}

static void testGetFrameNum(const char *title, int handle) {
	int result = sceMp3GetFrameNum(handle);
	if (result >= 0) {
		checkpoint("%s: %d", title, result);
	} else {
		checkpoint("%s: %08x", title, result);
	}
}

static void testGetFrameNum(const char *title, SceMp3InitArg *mp3Init, const u8 *header, int size) {
	int handle = initHeader(mp3Init, header, size);
	if (handle < 0) {
		checkpoint("%s: Failed (%08x)", title, handle);
	} else {
		testGetFrameNum(title, handle);
		sceMp3ReleaseMp3Handle(handle);
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
	testGetFrameNum("  Unreserved", 0);
	handle = sceMp3ReserveMp3Handle(NULL);
	testGetFrameNum("  NULL info arg", handle);
	sceMp3ReleaseMp3Handle(handle);
	handle = sceMp3ReserveMp3Handle(&mp3Init);
	testGetFrameNum("  Basic info arg", handle);
	sceMp3ReleaseMp3Handle(handle);
	testGetFrameNum("  Negative", -1);
	testGetFrameNum("  2", 2);

	static const u8 baseHeader[] = {
		0xFF, 0xFB, 0x10, 0x00,
	};
	static const u8 infoHeader[] = {
		0xFF, 0xFB, 0x10, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		'I', 'n', 'f', 'o',
		0x00, 0x00, 0x00, 0x01,
		0x01, 0x02, 0x03, 0x04,
	};
	static const u8 xingHeader[] = {
		0xFF, 0xFB, 0x10, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		'X', 'i', 'n', 'g',
		0x00, 0x00, 0x00, 0x01,
		0x01, 0x02, 0x03, 0x04,
	};
	static const u8 paddingHeader[] = {
		0xFF, 0xFB, 0x12, 0x00,
	};

	checkpointNext("After init");
	handle = sceMp3ReserveMp3Handle(NULL);
	sceMp3LowLevelInit(handle, 0);
	testGetFrameNum("  Low level init", handle);
	sceIoLseek32(fd, 0, SEEK_SET);
	sceIoRead(fd, mp3Buf, 4096);
	int consumed, written;
	sceMp3LowLevelDecode(handle, mp3Buf, &consumed, pcmBuf, &written);
	testGetFrameNum("  Low level decode", handle);
	sceMp3ReleaseMp3Handle(handle);
	handle = initHeader(&mp3Init, baseHeader, sizeof(baseHeader));
	testGetFrameNum("  After init", handle);
	sceMp3ReleaseMp3Handle(handle);
	handle = sceMp3ReserveMp3Handle(&mp3Init);
	u8 *dst;
	SceInt32 towrite, srcpos;
	if (sceMp3GetInfoToAddStreamData(handle, &dst, &towrite, &srcpos) >= 0) {
		sceIoLseek32(fd, srcpos, SEEK_SET);
		sceMp3NotifyAddStreamData(handle, sceIoRead(fd, dst, towrite));
		sceMp3Init(handle);
		sceMp3Decode(handle, &out);
		testGetFrameNum("  After decode", handle);
	}
	sceMp3ReleaseMp3Handle(handle);

	checkpointNext("Header info");
	handle = initHeader(&mp3Init, infoHeader, sizeof(infoHeader));
	testGetFrameNum("  Info header", handle);
	sceMp3ReleaseMp3Handle(handle);
	handle = initHeader(&mp3Init, xingHeader, sizeof(xingHeader));
	testGetFrameNum("  Xing header", handle);
	sceMp3ReleaseMp3Handle(handle);
	handle = initHeader(&mp3Init, baseHeader, sizeof(baseHeader), 1024);
	testGetFrameNum("  Offset header", handle);
	sceMp3ReleaseMp3Handle(handle);
	handle = initHeader(&mp3Init, paddingHeader, sizeof(paddingHeader));
	testGetFrameNum("  Padding header", handle);
	sceMp3ReleaseMp3Handle(handle);

	checkpointNext("Channels");
	static const char *channels[] = { "stereo", "joint stereo", "dual channel", "mono" };
	for (u32 bits = 0; bits < ARRAY_SIZE(channels); ++bits) {
		char temp[64];
		snprintf(temp, sizeof(temp), "  Channel %s", channels[bits]);
		u8 header[4];
		memcpy(header, baseHeader, sizeof(header));
		header[3] = bits << 6;
		handle = initHeader(&mp3Init, header, sizeof(header));
		if (handle < 0) {
			checkpoint("%s: Failed (%08x)", temp, handle);
		} else {
			testGetFrameNum(temp, handle);
		}
		sceMp3ReleaseMp3Handle(handle);
	}

	checkpointNext("Stream start:");
	mp3Init.mp3StreamStart = 103;
	testGetFrameNum("  Offset 103", &mp3Init, baseHeader, sizeof(baseHeader));
	mp3Init.mp3StreamStart = 104;
	testGetFrameNum("  Offset 104", &mp3Init, baseHeader, sizeof(baseHeader));
	mp3Init.mp3StreamStart = 207;
	testGetFrameNum("  Offset 207", &mp3Init, baseHeader, sizeof(baseHeader));
	mp3Init.mp3StreamStart = 208;
	testGetFrameNum("  Offset 208", &mp3Init, baseHeader, sizeof(baseHeader));
	mp3Init.mp3StreamStart = 1;
	testGetFrameNum("  Unaligned 1", &mp3Init, baseHeader, sizeof(baseHeader));
	mp3Init.mp3StreamStart = 0;

	checkpointNext("Stream end:");
	mp3Init.mp3StreamEnd = 1024;
	testGetFrameNum("  1024", &mp3Init, baseHeader, sizeof(baseHeader));
	mp3Init.mp3StreamEnd = 103;
	testGetFrameNum("  Too small", &mp3Init, baseHeader, sizeof(baseHeader));
	mp3Init.mp3StreamEnd = 104;
	testGetFrameNum("  Length 104", &mp3Init, baseHeader, sizeof(baseHeader));
	mp3Init.mp3StreamEnd = 105;
	testGetFrameNum("  Length 105", &mp3Init, baseHeader, sizeof(baseHeader));
	mp3Init.mp3StreamEnd = 1;
	testGetFrameNum("  Unaligned 1", &mp3Init, baseHeader, sizeof(baseHeader));
	mp3Init.mp3StreamEnd = 209;
	mp3Init.unk2 = 1;
	testGetFrameNum("  End value non-zero", &mp3Init, baseHeader, sizeof(baseHeader));
	mp3Init.unk2 = 0;
	mp3Init.mp3StreamEnd = sceIoLseek32(fd, 0, SEEK_END);

	handle = sceMp3ReserveMp3Handle(&mp3Init);
	sceMp3TermResource();
	checkpointNext("After term");
	testGetFrameNum("  Prev allocated handle", handle);

	return 0;
}
