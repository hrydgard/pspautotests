#include <common.h>
#include <pspmp3.h>
#include <pspsysmem.h>
#include <psputility.h>
#include <vector>

extern "C" int sceMp3LowLevelInit(int handle, int unk);

static u8 dummyMp3[128 * (1152 / 2)] __attribute__((aligned(64)));
static short pcmBuf[128 * (1152 / 2)] __attribute__((aligned(64)));

static void testReserve(const char *title, SceMp3InitArg *args) {
	int result = sceMp3ReserveMp3Handle(args);
	checkpoint("%s: %08x", title, result);
	if (result >= 0) {
		sceMp3ReleaseMp3Handle(result);
	}
}

static void testAllocation(SceMp3InitArg *args) {
	int handle;

	testReserve("  Once", args);
	testReserve("  Twice", args);
	handle = sceMp3ReserveMp3Handle(args);
	testReserve("  While allocated", args);
	sceMp3ReleaseMp3Handle(handle);

	std::vector<int> handles;
	do {
		handle = sceMp3ReserveMp3Handle(args);
		if (handle >= 0) {
			handles.push_back(handle);
		}
	} while (handle >= 0 && handles.size() < 1024);

	if (handles.empty()) {
		checkpoint("  Max %d handles, failed with %08x", handles.size(), handle);
	} else {
		checkpoint("  Max %d handles, last %d, failed with %08x", handles.size(), handles.back(), handle);
		for (size_t i = 0; i < handles.size(); ++i) {
			sceMp3ReleaseMp3Handle(handles[i]);
		}
	}
}

extern "C" int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_MP3);

	sceMp3InitResource();

	SceMp3InitArg mp3Init;
	mp3Init.mp3StreamStart = 0;
	mp3Init.mp3StreamEnd = sizeof(dummyMp3);
	mp3Init.unk1 = 0;
	mp3Init.unk2 = 0;
	mp3Init.mp3Buf = dummyMp3;
	mp3Init.mp3BufSize = sizeof(dummyMp3);
	mp3Init.pcmBuf = pcmBuf;
	mp3Init.pcmBufSize = sizeof(pcmBuf);

	int handle;
	checkpointNext("NULL arg:");
	testAllocation(NULL);
	handle = sceMp3ReserveMp3Handle(&mp3Init);
	testReserve("  While basic allocated", NULL);
	sceMp3ReleaseMp3Handle(handle);

	checkpointNext("Basic arg:");
	testAllocation(&mp3Init);
	handle = sceMp3ReserveMp3Handle(NULL);
	testReserve("  While NULL allocated", &mp3Init);
	sceMp3ReleaseMp3Handle(handle);

	checkpointNext("Stream start:");
	mp3Init.mp3StreamStart = sizeof(dummyMp3);
	testReserve("  End", &mp3Init);
	mp3Init.mp3StreamStart = 1;
	testReserve("  Unaligned 1", &mp3Init);
	mp3Init.mp3StreamStart = -1;
	testReserve("  Negative 1", &mp3Init);
	mp3Init.mp3StreamStart = 0;

	checkpointNext("Stream end:");
	mp3Init.mp3StreamEnd = 0;
	testReserve("  Zero", &mp3Init);
	mp3Init.mp3StreamStart = 64;
	mp3Init.mp3StreamEnd = 32;
	testReserve("  Before start", &mp3Init);
	mp3Init.mp3StreamStart = 0;
	mp3Init.mp3StreamEnd = 1;
	testReserve("  Unaligned 1", &mp3Init);
	mp3Init.mp3StreamEnd = -1;
	testReserve("  Negative 1", &mp3Init);
	mp3Init.mp3StreamEnd = sizeof(dummyMp3);

	checkpointNext("Unknown:");
	mp3Init.unk1 = 1;
	testReserve("  Start value non-zero", &mp3Init);
	mp3Init.unk1 = 0;
	mp3Init.unk2 = 1;
	testReserve("  End value non-zero", &mp3Init);
	mp3Init.unk2 = 0;
	mp3Init.mp3StreamStart = -1;
	mp3Init.unk2 = 1;
	testReserve("  End as 64 bit before start", &mp3Init);
	mp3Init.mp3StreamStart = 0;
	mp3Init.unk2 = 0;

	checkpointNext("Encoded buffer addr:");
	mp3Init.mp3Buf = dummyMp3 + 1;
	testReserve("  Unaligned +1", &mp3Init);
	mp3Init.mp3Buf = NULL;
	testReserve("  NULL", &mp3Init);
	mp3Init.mp3Buf = (void *)0x1337;
	testReserve("  Invalid", &mp3Init);
	mp3Init.mp3Buf = dummyMp3;

	checkpointNext("Encoded buffer size:");
	mp3Init.mp3BufSize = 8191;
	testReserve("  8191", &mp3Init);
	mp3Init.mp3BufSize = 8192;
	testReserve("  8192", &mp3Init);
	mp3Init.mp3BufSize = -1;
	testReserve("  -1", &mp3Init);
	mp3Init.mp3BufSize = 0x7FFFFFFF;
	testReserve("  0x7FFFFFFF", &mp3Init);
	mp3Init.mp3BufSize = sizeof(dummyMp3);

	checkpointNext("Decoded buffer addr:");
	mp3Init.pcmBuf = dummyMp3 + 1;
	testReserve("  Unaligned +1", &mp3Init);
	mp3Init.pcmBuf = NULL;
	testReserve("  NULL", &mp3Init);
	mp3Init.pcmBuf = (void *)0x1337;
	testReserve("  Invalid", &mp3Init);
	mp3Init.pcmBuf = dummyMp3;

	checkpointNext("Decoded buffer size:");
	mp3Init.pcmBufSize = 9215;
	testReserve("  9215", &mp3Init);
	mp3Init.pcmBufSize = 9216;
	testReserve("  9216", &mp3Init);
	mp3Init.pcmBufSize = -1;
	testReserve("  -1", &mp3Init);
	mp3Init.pcmBufSize = 0x7FFFFFFF;
	testReserve("  0x7FFFFFFF", &mp3Init);
	mp3Init.pcmBufSize = sizeof(pcmBuf);
}