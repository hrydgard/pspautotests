#include <common.h>
#include <pspiofilemgr.h>
#include <pspmp3.h>
#include <psputility.h>

// Where sceMp3ResetPlayPositionByFrame seeks to, as the srcpos the next sceMp3GetInfoToAddStreamData
// asks for. Frames are picked on both sides of where frame * bytes per second stops fitting in
// 32 bits, which was ~6 seconds into a 128kbps stream (PPSSPP issue #21442).

extern "C" {
#include <sysmem-imports.h>
SceInt32 sceMp3GetFrameNum(SceInt32 handle);
}

static u8 mp3Buf[8192] __attribute__((aligned(64)));
static short pcmBuf[4608] __attribute__((aligned(64)));

// Each stream is the same silent frame over and over, declared far longer than the data we ever
// supply: the seek is computed from the header, so only the start has to decode.
struct Stream {
	const char *name;
	u8 header[4];
	int frameSize;
	u32 start;
	// libmp3 refuses MPEG-2 and 2.5 from games built with an SDK before 6.00.
	bool needsSdk600;
};

static const Stream streams[] = {
	// MPEG-1, 128kbps, 44.1kHz, stereo. 417 bytes without padding.
	{ "MPEG-1 128kbps 44.1kHz", { 0xFF, 0xFB, 0x90, 0x04 }, 417, 0, false },
	// MPEG-1, 320kbps, 48kHz, joint stereo. Starts after a fake tag, as with an ID3 header.
	{ "MPEG-1 320kbps 48kHz, start 0x1234", { 0xFF, 0xFB, 0xE4, 0x44 }, 960, 0x1234, false },
	// MPEG-2, 64kbps, 24kHz, mono. Run with SDK 6.06.
	{ "MPEG-2 64kbps 24kHz", { 0xFF, 0xF3, 0x84, 0xC4 }, 192, 0, true },
	// MPEG-2, 32kbps, 16kHz, mono. Run with SDK 6.06.
	{ "MPEG-2 32kbps 16kHz", { 0xFF, 0xF3, 0x48, 0xC4 }, 144, 0, true },
};

static const u32 frames[] = {
	0, 1, 2, 3, 50, 93, 94, 100, 233, 234, 500, 1000, 1863, 1864, 5000, 20000,
};

static void fill(const Stream &s, u8 *dst, int size, int srcpos) {
	for (int i = 0; i < size; ++i) {
		int pos = srcpos + i - (int)s.start;
		if (pos < 0) {
			dst[i] = 0;
			continue;
		}
		int off = pos % s.frameSize;
		dst[i] = off < 4 ? s.header[off] : 0;
	}
}

static void feed(const Stream &s, int handle) {
	u8 *dst = NULL;
	SceInt32 towrite = 0;
	SceInt32 srcpos = 0;
	if (sceMp3GetInfoToAddStreamData(handle, &dst, &towrite, &srcpos) < 0 || towrite <= 0) {
		return;
	}
	fill(s, dst, towrite, srcpos);
	sceMp3NotifyAddStreamData(handle, towrite);
}

static void testStream(const Stream &s) {
	checkpointNext(s.name);

	SceMp3InitArg mp3Init;
	memset(&mp3Init, 0, sizeof(mp3Init));
	mp3Init.mp3StreamStart = s.start;
	// 8MB of stream, minutes long at any of these rates.
	mp3Init.mp3StreamEnd = s.start + 8 * 1024 * 1024;
	mp3Init.mp3Buf = (SceUChar8 *)mp3Buf;
	mp3Init.mp3BufSize = sizeof(mp3Buf);
	mp3Init.pcmBuf = (SceUChar8 *)pcmBuf;
	mp3Init.pcmBufSize = sizeof(pcmBuf);

	int handle = sceMp3ReserveMp3Handle(&mp3Init);
	if (handle < 0) {
		checkpoint("  sceMp3ReserveMp3Handle: %08x", handle);
		return;
	}

	feed(s, handle);
	int result = sceMp3Init(handle);
	checkpoint("  sceMp3Init: %08x", result);
	if (result < 0) {
		sceMp3ReleaseMp3Handle(handle);
		return;
	}
	int frameNum = sceMp3GetFrameNum(handle);
	checkpoint("  sceMp3GetFrameNum: %d", frameNum);

	for (size_t i = 0; i < ARRAY_SIZE(frames); ++i) {
		result = sceMp3ResetPlayPositionByFrame(handle, frames[i]);
		u8 *dst = NULL;
		SceInt32 towrite = -1;
		SceInt32 srcpos = -1;
		int info = sceMp3GetInfoToAddStreamData(handle, &dst, &towrite, &srcpos);
		checkpoint("  Frame %5d: %08x, info %08x, srcpos %08x (start+%d), towrite %d", frames[i], result, info, srcpos, srcpos - (int)s.start, towrite);
	}

	// Past the end, and the last frame.
	u32 edges[] = { (u32)frameNum - 1, (u32)frameNum, (u32)frameNum + 1, 0x80000000, 0xFFFFFFFF };
	for (size_t i = 0; i < ARRAY_SIZE(edges); ++i) {
		result = sceMp3ResetPlayPositionByFrame(handle, edges[i]);
		u8 *dst = NULL;
		SceInt32 towrite = -1;
		SceInt32 srcpos = -1;
		sceMp3GetInfoToAddStreamData(handle, &dst, &towrite, &srcpos);
		checkpoint("  Frame %08x: %08x, srcpos %08x", edges[i], result, srcpos);
	}

	// Decoding after a seek takes data from the new position.
	sceMp3ResetPlayPositionByFrame(handle, 1000);
	feed(s, handle);
	short *out = NULL;
	result = sceMp3Decode(handle, &out);
	checkpoint("  Decode after seek to 1000: %08x, sum decoded %d", result, sceMp3GetSumDecodedSample(handle));

	sceMp3ReleaseMp3Handle(handle);
}

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_MP3);
	sceMp3InitResource();

	// libmp3 only takes rates other than 44.1kHz from SDK 3.09.05 on, and from 6.00 it seeks by a
	// Xing or VBRI header when there is one. These streams have none either way.
	sceKernelSetCompiledSdkVersion500_505(0x05000010);
	for (size_t i = 0; i < ARRAY_SIZE(streams); ++i) {
		if (!streams[i].needsSdk600) {
			testStream(streams[i]);
		}
	}

	sceKernelSetCompiledSdkVersion606(0x06060010);
	for (size_t i = 0; i < ARRAY_SIZE(streams); ++i) {
		if (streams[i].needsSdk600) {
			testStream(streams[i]);
		}
	}

	sceMp3TermResource();
	return 0;
}
