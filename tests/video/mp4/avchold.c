// Which picture each sceMpegAvcDecode call hands back, and what sceMpegAvcDecodeStop hands back at
// the end, for streams that differ only in their headers. The decoder holds pictures back, and how
// many decides when a movie's last picture arrives - MGS Peace Walker's player hangs if it comes
// too late.
//
// What it shows: with HRD parameters and picture timing SEI, each picture comes out
// dpb_output_delay after its unit is decoded. Without, the decoder bumps pictures out of a buffer
// three frames deep (H.264 Annex C.4.5.3), whatever the stream says about reference frames,
// buffering or reordering - and with B-frames a call can then complete several pictures, of which
// only the first lands in the frame buffer, while status counts them all. More than three
// reference frames, and decoding fails. video/mpeg/decodeend's movie, from Sony's encoder, has
// picture timing with a one frame delay; remuxed into an MP4 and played this way, it held one
// picture back here too, so the PSMF and MP4 setups decode alike.
//
// The streams are in hold/, made by make_holdstreams.py. Every frame shows its own number in binary
// as bars across it, which is how the pictures that come out are identified.
#include <common.h>

#include <pspkernel.h>
#include <pspmpeg.h>
#include <psputility.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

typedef struct SceMp4Callbacks {
	void *param;
	SceOff (*tell)(void *param);
	SceOff (*seek)(void *param, SceOff offset, int whence);
	int (*read)(void *param, void *buf, int size);
} SceMp4Callbacks;

#define PSP_MODULE_AV_MP4 0x0308
#define MP4_TRACK_VIDEO 0x11

int sceMp4Init(int unk1, int unk2);
int sceMp4Finish(void);
int sceMp4Create(u32 *mp4, SceMp4Callbacks *callbacks, void *readBuffer, int readBufferSize);
int sceMp4Delete(u32 *mp4);
int sceMp4GetMovieInfo(u32 *mp4, u32 *info);
int sceMp4RegistTrack(u32 *mp4, int trackType, int unknown, SceMp4Callbacks *callbacks, void *track);
int sceMp4UnregistTrack(u32 *mp4, void *track);
int sceMp4TrackSampleBufQueryMemSize(int trackType, int numSamples, int maxSampleSize, int unknown, int readBufferSize);
int sceMp4TrackSampleBufConstruct(u32 *mp4, void *track, void *buffer, int memSize, int numSamples, int maxSampleSize, int unknown, int readBufferSize);
int sceMp4TrackSampleBufDestruct(u32 *mp4, void *track);
int sceMp4GetAvcTrackInfoData(u32 *mp4, void *track, u32 *info);
int sceMp4TrackSampleBufFlush(u32 *mp4, void *track);
int sceMp4PutSampleNum(u32 *mp4, void *track, int sample);
int sceMp4TrackSampleBufAvailableSize(u32 *mp4, void *track, int *writableSamples, int *writableBytes);
int sceMp4TrackSampleBufPut(u32 *mp4, void *track, int samples);
int sceMp4GetAvcAu(u32 *mp4, void *track, SceMpegAu *au, void *info);

int sceMpegAvcResourceInit(int unknown);
int sceMpegAvcResourceFinish(void);
void *sceMpegAvcResourceGetAvcDecTopAddr(void);
void *sceMpegAvcResourceGetAvcEsBuf(void);

#define MAX_CALLS 200
#define STOP_BUFS 8
#define MAX_FILE_SIZE (320 * 1024)

// The frame buffers come from the partition rather than the heap, so leave room for them.
unsigned int sce_newlib_heap_kb_size = 1024;

typedef struct {
	const char *name;
	int width, height;
} Stream;

// The same list as make_holdstreams.py.
static const Stream g_streams[] = {
	{ "ref1", 480, 272 }, { "ref2", 480, 272 }, { "ref3", 480, 272 }, { "ref4", 480, 272 }, { "ref5", 480, 272 },
	{ "ref1norestrict", 480, 272 }, { "ref3norestrict", 480, 272 },
	{ "ref1buf3", 480, 272 }, { "ref3buf4", 480, 272 }, { "ref3reorder2", 480, 272 },
	{ "ref3b1", 480, 272 }, { "ref3b2", 480, 272 },
	{ "s144", 144, 80 }, { "s240", 240, 136 }, { "s320", 320, 176 }, { "s480l21", 480, 272 },
	{ "ref1aud", 480, 272 }, { "ref1hrd", 480, 272 }, { "ref1pic", 480, 272 },
	{ "ref1hrdpic", 480, 272 }, { "ref3b1hrd", 480, 272 },
};
static const Stream *g_stream;

static u8 *g_file;
static int g_fileSize;
static int g_filePos;

static SceOff fileTell(void *param) {
	return g_filePos;
}

static SceOff fileSeek(void *param, SceOff offset, int whence) {
	SceOff pos = whence == PSP_SEEK_SET ? offset : whence == PSP_SEEK_CUR ? g_filePos + offset : g_fileSize + offset;
	if (pos < 0 || pos > g_fileSize) {
		return -1;
	}
	g_filePos = (int)pos;
	return pos;
}

static int fileRead(void *param, void *buf, int size) {
	if (size > g_fileSize - g_filePos) {
		size = g_fileSize - g_filePos;
	}
	memcpy(buf, g_file + g_filePos, size);
	g_filePos += size;
	return size;
}

static SceMp4Callbacks g_callbacks = { NULL, fileTell, fileSeek, fileRead };

static u32 g_mp4[64] __attribute__((aligned(64)));
// sceMp4TrackSampleBufConstruct writes at least 0x13CC bytes into the track, not the 4KB it looks
// like it needs.
static u8 g_videoTrack[16384] __attribute__((aligned(64)));
static u32 g_auInfo[16];
static SceMpeg g_mpeg;
static SceMpegAu g_videoAu;

static void *g_frameBuf;
static void *g_stopBufs[STOP_BUFS];
// The picture each decode call returned: its frame number, or -1 for none, and the status it gave.
static int g_out[MAX_CALLS];
static int g_status[MAX_CALLS];

// The decoder writes pictures by DMA, so they're read through the uncached mirror.
static u32 *uncached(void *buf) {
	return (u32 *)((u32)buf | 0x40000000);
}

// Fills the row frameNumber samples with mid grey, which reads back as frame 255, so a call that
// reports a picture without writing one shows up.
static void clearRow(void *buf) {
	u32 *row = uncached(buf) + (g_stream->height / 2) * 512;
	int x;
	for (x = 0; x < 512; x++) {
		row[x] = 0xFF808080;
	}
}

static void *allocHigh(int size) {
	SceUID id = sceKernelAllocPartitionMemory(2, "avchold", PSP_SMEM_High, size + 64, NULL);
	if (id < 0) {
		printf("sceKernelAllocPartitionMemory(%d): %08x\n", size, id);
		return NULL;
	}
	return (void *)(((u32)sceKernelGetBlockHeadAddr(id) + 63) & ~63);
}

// Reads the frame number off a decoded picture: bar b (an eighth of the width) is white if bit b
// is set.
static int frameNumber(void *buf) {
	const u32 *row = uncached(buf) + (g_stream->height / 2) * 512;
	const int bar = g_stream->width / 8;
	int n = 0, b;
	for (b = 0; b < 8; b++) {
		u32 px = row[bar / 2 + bar * b];
		int g = (px >> 8) & 0xFF;
		if (g >= 128) {
			n |= 1 << b;
		}
	}
	return n;
}

// Prints the calls as runs: "none" for calls without a picture, and "calls a-b: frames c-d" where
// both step by one together. A status other than 0 or 1 is printed, and splits a run.
static void printRuns(int calls) {
	int i = 0;
	while (i < calls) {
		int j = i + 1;
		if (g_out[i] < 0) {
			while (j < calls && g_out[j] < 0 && g_status[j] == g_status[i]) {
				j++;
			}
			printf("  calls %d-%d: none", i, j - 1);
		} else {
			while (j < calls && g_out[j] == g_out[j - 1] + 1 && g_status[j] == g_status[i]) {
				j++;
			}
			printf("  calls %d-%d: frames %d-%d", i, j - 1, g_out[i], g_out[j - 1]);
		}
		if (g_status[i] != 0 && g_status[i] != 1) {
			printf(" (status %d)", g_status[i]);
		}
		printf("\n");
		i = j;
	}
}

static int loadFile(const char *name) {
	char path[64];
	snprintf(path, sizeof(path), "hold/%s.mp4", name);
	SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
	if (fd < 0) {
		printf("%s: can't open %s: %08x\n", name, path, fd);
		return -1;
	}
	g_fileSize = sceIoLseek32(fd, 0, PSP_SEEK_END);
	if (g_fileSize > MAX_FILE_SIZE) {
		printf("%s: %d bytes is too big\n", name, g_fileSize);
		sceIoClose(fd);
		return -1;
	}
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	sceIoRead(fd, g_file, g_fileSize);
	sceIoClose(fd);
	g_filePos = 0;
	return 0;
}

static void testStream(const char *name, void *mpegData, int mpegSize, void *decTop, void *esBuf, void *trackBuf, int trackSize, void *readBuf) {
	if (loadFile(name) < 0) {
		return;
	}

	int result = sceMpegCreate(&g_mpeg, mpegData, mpegSize, NULL, 512, 1, (int)decTop);
	if (result != 0) {
		printf("%s: sceMpegCreate: %08x\n", name, result);
		return;
	}
	result = sceMp4Create(g_mp4, &g_callbacks, readBuf, 0x1000);
	if (result != 0) {
		printf("%s: sceMp4Create: %08x\n", name, result);
		sceMpegDelete(&g_mpeg);
		return;
	}
	u32 movieInfo[16] = {0};
	sceMp4GetMovieInfo(g_mp4, movieInfo);
	sceMp4RegistTrack(g_mp4, MP4_TRACK_VIDEO, 0, &g_callbacks, g_videoTrack);
	sceMp4TrackSampleBufConstruct(g_mp4, g_videoTrack, trackBuf, trackSize, 30, 20020, 98000, 0x10000);
	sceMpegInitAu(&g_mpeg, esBuf, &g_videoAu);
	u32 info[16] = {0};
	sceMp4GetAvcTrackInfoData(g_mp4, g_videoTrack, info);
	const int samples = (int)info[2];
	sceMp4TrackSampleBufFlush(g_mp4, g_videoTrack);
	sceMp4PutSampleNum(g_mp4, g_videoTrack, 0);

	int calls = 0, pictures = 0;
	while (calls < samples && calls < MAX_CALLS) {
		int writable = 0, bytes = 0;
		if (sceMp4TrackSampleBufAvailableSize(g_mp4, g_videoTrack, &writable, &bytes) == 0 && writable > 0) {
			sceMp4TrackSampleBufPut(g_mp4, g_videoTrack, writable);
		}
		result = sceMp4GetAvcAu(g_mp4, g_videoTrack, &g_videoAu, g_auInfo);
		if (result != 0) {
			printf("  sceMp4GetAvcAu at call %d: %08x\n", calls, result);
			break;
		}
		SceInt32 status = 0;
		clearRow(g_frameBuf);
		result = sceMpegAvcDecode(&g_mpeg, &g_videoAu, 512, &g_frameBuf, &status);
		if (result != 0) {
			printf("  sceMpegAvcDecode at call %d: %08x\n", calls, result);
			break;
		}
		g_out[calls] = status != 0 ? frameNumber(g_frameBuf) : -1;
		g_status[calls] = (int)status;
		if (status != 0) {
			pictures++;
		}
		calls++;
	}

	printf("%s: %d samples, %d decodes, %d pictures\n", name, samples, calls, pictures);
	printRuns(calls);

	// Stopping a decoder that never decoded anything crashes mpeg.prx.
	if (calls == 0) {
		sceMp4TrackSampleBufDestruct(g_mp4, g_videoTrack);
		sceMp4UnregistTrack(g_mp4, g_videoTrack);
		sceMp4Delete(g_mp4);
		sceMpegDelete(&g_mpeg);
		return;
	}

	int i;
	for (i = 0; i < STOP_BUFS; i++) {
		clearRow(g_stopBufs[i]);
	}
	SceInt32 status = -1;
	result = sceMpegAvcDecodeStop(&g_mpeg, 512, g_stopBufs, &status);
	printf("  sceMpegAvcDecodeStop: %08x, status %d, frames", result, (int)status);
	for (i = 0; i < status && i < STOP_BUFS; i++) {
		printf(" %d", frameNumber(g_stopBufs[i]));
	}
	printf("\n");
	status = -1;
	result = sceMpegAvcDecodeStop(&g_mpeg, 512, g_stopBufs, &status);
	printf("  again: %08x, status %d\n", result, (int)status);

	sceMp4TrackSampleBufDestruct(g_mp4, g_videoTrack);
	sceMp4UnregistTrack(g_mp4, g_videoTrack);
	sceMp4Delete(g_mp4);
	sceMpegDelete(&g_mpeg);
}

// The harness echoes stdout to the debug screen otherwise.
extern unsigned int HAS_DISPLAY;

int main(int argc, char *argv[]) {
	HAS_DISPLAY = 0;
	if (sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC) < 0 || sceUtilityLoadModule(PSP_MODULE_AV_MPEGBASE) < 0 ||
		sceUtilityLoadModule(PSP_MODULE_AV_MP4) < 0) {
		printf("Could not load the AV modules\n");
		return 1;
	}

	g_file = allocHigh(MAX_FILE_SIZE);
	g_frameBuf = allocHigh(512 * 272 * 4);
	int i;
	for (i = 0; i < STOP_BUFS; i++) {
		g_stopBufs[i] = allocHigh(512 * 272 * 4);
	}

	sceMpegInit();
	sceMpegAvcResourceInit(1);
	void *decTop = sceMpegAvcResourceGetAvcDecTopAddr();
	void *esBuf = sceMpegAvcResourceGetAvcEsBuf();
	int mpegSize = sceMpegQueryMemSize(1);
	void *mpegData = allocHigh(mpegSize);
	sceMp4Init(1, 1);
	void *readBuf = memalign(64, 0x1000);
	int trackSize = sceMp4TrackSampleBufQueryMemSize(MP4_TRACK_VIDEO, 30, 20020, 98000, 0x10000);
	void *trackBuf = allocHigh(trackSize);

	for (i = 0; i < (int)(sizeof(g_streams) / sizeof(g_streams[0])); i++) {
		g_stream = &g_streams[i];
		testStream(g_streams[i].name, mpegData, mpegSize, decTop, esBuf, trackBuf, trackSize, readBuf);
	}

	sceMp4Finish();
	sceMpegAvcResourceFinish();
	sceMpegFinish();
	return 0;
}
