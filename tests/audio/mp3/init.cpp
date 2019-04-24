#include <common.h>
#include <pspiofilemgr.h>
#include <pspmp3.h>
#include <psputility.h>
#include <psputils.h>

static u8 mp3Buf[8192] __attribute__((aligned(64)));
static short pcmBuf[4608] __attribute__((aligned(64)));

static void testInit(const char *title, int handle) {
	int result = sceMp3Init(handle);
	checkpoint("%s: %08x", title, result);
}

static void testInitHeader(const char *title, SceMp3InitArg *mp3Init, const u8 *header, int size, int offset = 0) {
	memset(mp3Buf, 0, sizeof(mp3Buf));
	int handle = sceMp3ReserveMp3Handle(mp3Init);
	u8 *dst = NULL;
	sceMp3GetInfoToAddStreamData(handle, &dst, NULL, NULL);
	memcpy(dst + offset, header, size);
	sceKernelDcacheWritebackInvalidateRange(mp3Buf, sizeof(mp3Buf));

	int result = sceMp3Init(handle);
	checkpoint("%s: %08x", title, result);
	sceMp3ReleaseMp3Handle(handle);
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
	testInit("  Unreserved", 0);
	handle = sceMp3ReserveMp3Handle(NULL);
	testInit("  NULL info arg", handle);
	sceMp3ReleaseMp3Handle(handle);
	handle = sceMp3ReserveMp3Handle(&mp3Init);
	testInit("  Basic info arg", handle);
	sceMp3ReleaseMp3Handle(handle);
	testInit("  Negative", -1);
	testInit("  2", 2);

	handle = sceMp3ReserveMp3Handle(&mp3Init);
	sceMp3GetInfoToAddStreamData(handle, &dst, &towrite, &srcpos);
	sceIoLseek32(fd, srcpos, SEEK_SET);
	sceIoRead(fd, dst, towrite);
	sceMp3NotifyAddStreamData(handle, towrite);
	checkpointNext("Repeating");
	testInit("  Once", handle);
	testInit("  Twice", handle);
	sceMp3ReleaseMp3Handle(handle);

	checkpointNext("Header");

	static const u8 properHeader[] = {
		0xFF, 0xFB, 0x10, 0x00,
	};
	static const u8 nosyncHeader[] = {
		0x00, 0x0B, 0x10, 0x00,
	};
	static const u8 allMissingHeader[] = {
		0x00, 0x00, 0x00, 0x00,
	};
	testInitHeader("  Proper header", &mp3Init, properHeader, sizeof(properHeader));
	testInitHeader("  Sync missing", &mp3Init, nosyncHeader, sizeof(nosyncHeader));
	testInitHeader("  All missing", &mp3Init, allMissingHeader, sizeof(allMissingHeader));

	static const u8 baseHeader[] = {
		0xFF, 0xE1, 0x10, 0x00,
	};
	static const u8 versions[] = { 3, 2, 0, 1 };
	static const char *versionNames[] = { "v1", "v2", "v2.5", "invalid" };
	static const u8 layers[] = { 3, 2, 1, 0 };
	static const char *layerNames[] = { "I", "II", "III", "invalid" };

	for (int v = 0; v <= 3; ++v) {
		for (int l = 0; l <= 3; ++l) {
			u8 header[4];
			memcpy(header, baseHeader, sizeof(header));
			header[1] |= (versions[v] << 3) | (layers[l] << 1);

			char temp[64];
			snprintf(temp, sizeof(temp), "  MPEG %s layer %s", versionNames[v], layerNames[l]);
			testInitHeader(temp, &mp3Init, header, sizeof(header));
		}
	}

	static const u8 crcHeader[] = {
		0xFF, 0xFA, 0x10, 0x00,
	};
	static const u8 paddingHeader[] = {
		0xFF, 0xFB, 0x12, 0x00,
	};
	static const u8 privateHeader[] = {
		0xFF, 0xFB, 0x11, 0x00,
	};
	static const u8 copyrightHeader[] = {
		0xFF, 0xFB, 0x10, 0x08,
	};
	static const u8 originalBitHeader[] = {
		0xFF, 0xFB, 0x10, 0x04,
	};
	testInitHeader("  CRC protection", &mp3Init, crcHeader, sizeof(crcHeader));
	testInitHeader("  Padding bit", &mp3Init, paddingHeader, sizeof(paddingHeader));
	testInitHeader("  Private bit", &mp3Init, privateHeader, sizeof(privateHeader));
	testInitHeader("  Copyright bit", &mp3Init, copyrightHeader, sizeof(copyrightHeader));
	testInitHeader("  Original bit", &mp3Init, originalBitHeader, sizeof(originalBitHeader));

	static const u8 bitrateFreeHeader[] = {
		0xFF, 0xFA, 0x00, 0x00,
	};
	static const u8 bitrateMaxHeader[] = {
		0xFF, 0xFA, 0xE0, 0x00,
	};
	static const u8 bitrateInvalidHeader[] = {
		0xFF, 0xFA, 0xF0, 0x00,
	};
	testInitHeader("  Free bitrate", &mp3Init, bitrateFreeHeader, sizeof(bitrateFreeHeader));
	testInitHeader("  Max bitrate", &mp3Init, bitrateMaxHeader, sizeof(bitrateMaxHeader));
	testInitHeader("  Invalid bitrate", &mp3Init, bitrateInvalidHeader, sizeof(bitrateInvalidHeader));

	static const u8 samplerate48Header[] = {
		0xFF, 0xFB, 0x14, 0x00,
	};
	static const u8 samplerate32Header[] = {
		0xFF, 0xFB, 0x18, 0x00,
	};
	static const u8 samplerateInvalidHeader[] = {
		0xFF, 0xFB, 0x1C, 0x00,
	};
	testInitHeader("  48kHz sample rate", &mp3Init, samplerate48Header, sizeof(samplerate48Header));
	testInitHeader("  32kHz sample rate", &mp3Init, samplerate32Header, sizeof(samplerate32Header));
	testInitHeader("  Invalid sample rate", &mp3Init, samplerateInvalidHeader, sizeof(samplerateInvalidHeader));

	static const u8 jointStereoHeader[] = {
		0xFF, 0xFB, 0x10, 0x40,
	};
	static const u8 dualChannelHeader[] = {
		0xFF, 0xFB, 0x10, 0x80,
	};
	static const u8 monoChannelHeader[] = {
		0xFF, 0xFB, 0x10, 0xC0,
	};
	static const u8 jointExtendedHeader[] = {
		0xFF, 0xFB, 0x10, 0x70,
	};
	testInitHeader("  Joint stereo", &mp3Init, jointStereoHeader, sizeof(jointStereoHeader));
	testInitHeader("  Dual channel", &mp3Init, dualChannelHeader, sizeof(dualChannelHeader));
	testInitHeader("  Mono", &mp3Init, monoChannelHeader, sizeof(monoChannelHeader));
	testInitHeader("  Joint stereo extended", &mp3Init, jointExtendedHeader, sizeof(jointExtendedHeader));

	static const u8 emphasis5015Header[] = {
		0xFF, 0xFB, 0x10, 0x01,
	};
	static const u8 emphasisCCITHeader[] = {
		0xFF, 0xFB, 0x10, 0x03,
	};
	static const u8 emphasisInvalidHeader[] = {
		0xFF, 0xFB, 0x10, 0x02,
	};
	testInitHeader("  Emphasis 50/15ms", &mp3Init, emphasis5015Header, sizeof(emphasis5015Header));
	testInitHeader("  Emphasis CCIT", &mp3Init, emphasisCCITHeader, sizeof(emphasisCCITHeader));
	testInitHeader("  Emphasis invalid", &mp3Init, emphasisInvalidHeader, sizeof(emphasisInvalidHeader));

	checkpointNext("Header offset");
	static const int offsets[] = { 1, 2, 3, 4, 256, 1024, 1439, 1440, 2048, 4096 };
	for (size_t i = 0; i < ARRAY_SIZE(offsets); ++i) {
		char temp[64];
		snprintf(temp, sizeof(temp), "  Offset %d", offsets[i]);
		testInitHeader(temp, &mp3Init, properHeader, sizeof(properHeader), offsets[i]);
	}

	u8 offsetHeader[8];
	memcpy(offsetHeader + 0, bitrateFreeHeader, sizeof(bitrateFreeHeader));
	memcpy(offsetHeader + 4, properHeader, sizeof(properHeader));
	testInitHeader("  Valid after invalid bitrate", &mp3Init, offsetHeader, sizeof(offsetHeader), 0);
	memcpy(offsetHeader + 0, baseHeader, sizeof(baseHeader));
	offsetHeader[1] |= (versions[2] << 3) | (layers[2] << 1);
	testInitHeader("  Valid after MPEG v2.5 layer III", &mp3Init, offsetHeader, sizeof(offsetHeader), 0);
	memcpy(offsetHeader + 0, baseHeader, sizeof(baseHeader));
	offsetHeader[1] |= (versions[1] << 3) | (layers[2] << 1);
	testInitHeader("  Valid after MPEG v2 layer III", &mp3Init, offsetHeader, sizeof(offsetHeader), 0);
	offsetHeader[1] |= (versions[0] << 3) | (layers[0] << 1);
	testInitHeader("  Valid after MPEG v1 layer I", &mp3Init, offsetHeader, sizeof(offsetHeader), 0);

	handle = sceMp3ReserveMp3Handle(&mp3Init);
	sceMp3GetInfoToAddStreamData(handle, &dst, &towrite, &srcpos);
	sceIoLseek32(fd, srcpos, SEEK_SET);
	sceIoRead(fd, dst, towrite);
	sceMp3NotifyAddStreamData(handle, towrite);
	checkpointNext("Sequence");
	testInit("  Initial init", handle);
	short *out;
	sceMp3Decode(handle, &out);
	testInit("  After decode", handle);
	sceMp3ReleaseMp3Handle(handle);

	handle = sceMp3ReserveMp3Handle(&mp3Init);
	sceMp3TermResource();
	checkpointNext("After term");
	testInit("  Prev allocated handle", handle);
}
