// Plays sample.mp4 (H.264 + AAC) with sceMp4, and times each step of the pipeline.
//
// This is both a sample of how to play an MP4 on the PSP, and a timing probe for how long the
// firmware's decoders take, which the emulator's cost model is based on. It is not a pass/fail
// test: its output is timings.
//
// How the pieces fit together (as in Meruru no Atelier Plus Official PlayView, which the call
// sequence and parameters are taken from):
//  - sceMp4 (libmp4.prx + mp4msv.prx) parses the MP4 and hands out access units (AUs). It reads
//    the file through three callbacks we supply, so the data can come from anywhere.
//  - Video AUs are decoded by mpeg.prx's AVC decoder, sceMpegAvcDecode, set up with the
//    sceMpegAvcResource functions rather than a PSMF ringbuffer.
//  - Audio AUs are decoded by sceMp4 itself (sceMp4AacDecode), 1024 stereo samples per AU.
//
// sample.mp4 is made by make_sample.sh and is synthetic, so free of copyright.
#include <common.h>

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <pspaudio.h>
#include <psppower.h>
#include <psputility.h>
#include <pspmpeg.h>

#include <stdio.h>
#include <string.h>
#include <malloc.h>

#define PSP_MODULE_AV_MP4 0x0308

// sceMpegAvcResourceInit takes a fixed range of user memory starting at 0x08C00000 for the AVC
// decoder, and fails quietly (GetAvcDecTopAddr/GetAvcEsBuf return 0) if anything is there. The
// default newlib heap covers it, so keep the heap small and take the big buffers from the top of
// user memory instead (allocHigh).
PSP_HEAP_SIZE_KB(1024);

static void *allocHigh(int size) {
	SceUID id = sceKernelAllocPartitionMemory(2, "mp4timing", PSP_SMEM_High, size + 64, NULL);
	if (id < 0) {
		printf("sceKernelAllocPartitionMemory(%d): %08x\n", size, id);
		return NULL;
	}
	return (void *)(((u32)sceKernelGetBlockHeadAddr(id) + 63) & ~63);
}

// sceMp4 isn't in pspsdk. Signatures as used by the firmware and by games.
typedef struct SceMp4Callbacks {
	void *param;
	SceOff (*tell)(void *param);
	SceOff (*seek)(void *param, SceOff offset, int whence);
	int (*read)(void *param, void *buf, int size);
} SceMp4Callbacks;

#define MP4_TRACK_VIDEO 0x11
#define MP4_TRACK_AUDIO 0x20

int sceMp4Init(int unk1, int unk2);
int sceMp4Finish(void);
int sceMp4Create(u32 *mp4, SceMp4Callbacks *callbacks, void *readBuffer, int readBufferSize);
int sceMp4Delete(u32 *mp4);
int sceMp4GetMovieInfo(u32 *mp4, u32 *info);  // [0] number of tracks, [2] duration
int sceMp4GetNumberOfSpecificTrack(u32 *mp4, int trackType);
int sceMp4RegistTrack(u32 *mp4, int trackType, int unknown, SceMp4Callbacks *callbacks, void *track);
int sceMp4UnregistTrack(u32 *mp4, void *track);
int sceMp4TrackSampleBufQueryMemSize(int trackType, int numSamples, int maxSampleSize, int unknown, int readBufferSize);
int sceMp4TrackSampleBufConstruct(u32 *mp4, void *track, void *buffer, int memSize, int numSamples, int maxSampleSize, int unknown, int readBufferSize);
int sceMp4TrackSampleBufDestruct(u32 *mp4, void *track);
int sceMp4GetAvcTrackInfoData(u32 *mp4, void *track, u32 *info);  // [1] duration, [2] samples
int sceMp4GetAacTrackInfoData(u32 *mp4, void *track, u32 *info);  // + [3] sample rate, [4] channels
int sceMp4TrackSampleBufFlush(u32 *mp4, void *track);
int sceMp4PutSampleNum(u32 *mp4, void *track, int sample);
int sceMp4TrackSampleBufAvailableSize(u32 *mp4, void *track, int *writableSamples, int *writableBytes);
int sceMp4TrackSampleBufPut(u32 *mp4, void *track, int samples);
int sceMp4GetAvcAu(u32 *mp4, void *track, SceMpegAu *au, void *info);
int sceMp4GetAacAu(u32 *mp4, void *track, SceMpegAu *au, void *info);
int sceMp4AacDecodeInitResource(int unknown);
int sceMp4AacDecodeInit(void *aac);
int sceMp4AacDecode(void *aac, SceMpegAu *au, void *out, int init, int frequency);
int sceMp4AacDecodeExit(void *aac);
int sceMp4AacDecodeTermResource(void);

int sceMpegAvcResourceInit(int unknown);
int sceMpegAvcResourceFinish(void);
void *sceMpegAvcResourceGetAvcDecTopAddr(void);
void *sceMpegAvcResourceGetAvcEsBuf(void);

// Progress log, flushed each time so that if something hangs on hardware, the output file shows
// the last step reached.
#define STEP(...) do { printf("step %6d ms: ", (int)((sceKernelGetSystemTimeLow() - g_t0) / 1000)); printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)
static u32 g_t0;
// The test harness echoes stdout to the debug screen, which would draw over the video.
extern unsigned int HAS_DISPLAY;

#define AUDIO_BUFS 8
#define AUDIO_SAMPLES 1024
#define MAX_FRAMES 600

// The file, served to sceMp4 from memory. A game would call sceIoRead/sceIoLseek here instead:
// the callbacks have exactly their signatures.
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

// sceMp4Create fills in about 0x90 bytes here, and RegistTrack/TrackSampleBufConstruct at least 184
// per track, so leave plenty of room.
static u32 g_mp4[64] __attribute__((aligned(64)));
static u8 g_videoTrack[4096] __attribute__((aligned(64)));
static u8 g_audioTrack[4096] __attribute__((aligned(64)));
static u32 g_aac[16] __attribute__((aligned(64)));
static SceMpeg g_mpeg;
static SceMpegAu g_videoAu, g_audioAu;
static u32 g_auInfo[16];

static void *g_audioBufs[AUDIO_BUFS];
static volatile int g_audioDecoded;
static volatile int g_audioPlayed;
static volatile int g_quit;

static unsigned int __attribute__((aligned(64))) g_list[64 * 1024];
static void *g_frameBuf;
static void *g_stopBufs[4];
static int g_drawBuf;

// Timings, all in microseconds.
static int g_getAvcUs[MAX_FRAMES], g_avcDecodeUs[MAX_FRAMES], g_blitUs[MAX_FRAMES];
static u32 g_flipTime[MAX_FRAMES];
static long long g_aacUs, g_aacGetUs, g_putUs;
static int g_aacMax, g_putMax, g_puts;
static int g_frames;

static int soundThread(SceSize args, void *argp) {
	while (!g_quit) {
		if (g_audioPlayed < g_audioDecoded) {
			sceAudioOutput2OutputBlocking(0x8000, g_audioBufs[g_audioPlayed % AUDIO_BUFS]);
			g_audioPlayed++;
		} else {
			sceKernelDelayThread(1000);
		}
	}
	return 0;
}

// Keeps a track's sample buffer full. Put reads the file through the callbacks.
static void topUp(void *track) {
	int samples = 0, bytes = 0;
	if (sceMp4TrackSampleBufAvailableSize(g_mp4, track, &samples, &bytes) == 0 && samples > 0) {
		u32 t0 = sceKernelGetSystemTimeLow();
		sceMp4TrackSampleBufPut(g_mp4, track, samples);
		int dt = (int)(sceKernelGetSystemTimeLow() - t0);
		g_putUs += dt;
		if (dt > g_putMax) g_putMax = dt;
		g_puts++;
	}
}

// Draws the decoded frame in 32-pixel strips (a single wide sprite from a linear texture thrashes
// the texture cache and is several times slower), then flips on the next vblank.
static void present() {
	sceGuStart(GU_DIRECT, g_list);
	sceGuDrawBufferList(GU_PSM_8888, (void *)(g_drawBuf ? 0x88000 : 0), 512);
	sceGuTexMode(GU_PSM_8888, 0, 0, 0);
	sceGuTexImage(0, 512, 512, 512, g_frameBuf);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
	sceGuTexFilter(GU_NEAREST, GU_NEAREST);
	sceGuEnable(GU_TEXTURE_2D);
	sceGuDisable(GU_DEPTH_TEST);
	sceGuDisable(GU_BLEND);
	typedef struct { u16 u, v; s16 x, y, z; } V;
	int strip;
	for (strip = 0; strip < 15; strip++) {
		V *v = (V *)sceGuGetMemory(2 * sizeof(V));
		v[0].u = strip * 32; v[0].v = 0; v[0].x = strip * 32; v[0].y = 0; v[0].z = 0;
		v[1].u = strip * 32 + 32; v[1].v = 272; v[1].x = strip * 32 + 32; v[1].y = 272; v[1].z = 0;
		sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
	}
	sceGuFinish();
	sceGuSync(0, 0);
}

static void flip() {
	sceDisplaySetFrameBuf((void *)(0x04000000 + (g_drawBuf ? 0x88000 : 0)), 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
	g_drawBuf ^= 1;
}

static void printTimes(const char *name, const int *us, int count) {
	long long sum = 0;
	int minV = 0x7FFFFFFF, maxV = 0, i;
	for (i = 0; i < count; i++) {
		sum += us[i];
		if (us[i] < minV) minV = us[i];
		if (us[i] > maxV) maxV = us[i];
	}
	printf("%-20s avg %6d us, min %6d, max %6d\n", name, count ? (int)(sum / count) : 0, count ? minV : 0, maxV);
}

int main(int argc, char *argv[]) {
	HAS_DISPLAY = 0;
	g_t0 = sceKernelGetSystemTimeLow();
	// Codecs are loaded through sceUtility, the way games do: 0x300 is the ME codec driver, 0x303
	// brings in mpeg.prx, 0x306 AAC, 0x308 libmp4 + mp4msv.
	if (sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC) < 0 || sceUtilityLoadModule(PSP_MODULE_AV_MPEGBASE) < 0 ||
		sceUtilityLoadModule(PSP_MODULE_AV_AAC) < 0 || sceUtilityLoadModule(PSP_MODULE_AV_MP4) < 0) {
		printf("Could not load the AV modules\n");
		return 1;
	}

	STEP("modules loaded");
	SceUID fd = sceIoOpen("sample.mp4", PSP_O_RDONLY, 0777);
	if (fd < 0) {
		printf("Can't open sample.mp4: %08x\n", fd);
		return 1;
	}
	g_fileSize = sceIoLseek32(fd, 0, PSP_SEEK_END);
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	g_file = allocHigh(g_fileSize);
	sceIoRead(fd, g_file, g_fileSize);
	sceIoClose(fd);

	// The AVC decoder, without a PSMF ringbuffer.
	STEP("file read, %d bytes", g_fileSize);
	sceMpegInit();
	sceMpegAvcResourceInit(1);
	STEP("mpeg init + avc resource init");
	void *decTop = sceMpegAvcResourceGetAvcDecTopAddr();
	void *esBuf = sceMpegAvcResourceGetAvcEsBuf();
	STEP("dectop %08x esbuf %08x", (unsigned)decTop, (unsigned)esBuf);
	int mpegSize = sceMpegQueryMemSize(1);
	void *mpegData = allocHigh(mpegSize);
	int result = sceMpegCreate(&g_mpeg, mpegData, mpegSize, NULL, 512, 1, (int)decTop);
	if (result != 0) {
		printf("sceMpegCreate: %08x\n", result);
		return 1;
	}

	// The container.
	STEP("sceMpegCreate ok");
	sceMp4Init(1, 1);
	void *readBuf = memalign(64, 0x1000);
	result = sceMp4Create(g_mp4, &g_callbacks, readBuf, 0x1000);
	if (result != 0) {
		printf("sceMp4Create: %08x\n", result);
		return 1;
	}
	STEP("sceMp4Create ok");
	u32 movieInfo[16] = {0};
	sceMp4GetMovieInfo(g_mp4, movieInfo);

	// The video track, with room for 30 samples of up to 20020 bytes (the sizes the PlayView app uses).
	STEP("movie info: %d tracks", (int)movieInfo[0]);
	sceMp4RegistTrack(g_mp4, MP4_TRACK_VIDEO, 0, &g_callbacks, g_videoTrack);
	int vSize = sceMp4TrackSampleBufQueryMemSize(MP4_TRACK_VIDEO, 30, 20020, 98000, 0x10000);
	void *vBuf = allocHigh(vSize);
	sceMp4TrackSampleBufConstruct(g_mp4, g_videoTrack, vBuf, vSize, 30, 20020, 98000, 0x10000);
	sceMpegInitAu(&g_mpeg, esBuf, &g_videoAu);
	u32 videoInfo[16] = {0};
	sceMp4GetAvcTrackInfoData(g_mp4, g_videoTrack, videoInfo);

	// The audio track, and the AAC decoder.
	STEP("video track set up");
	sceMp4RegistTrack(g_mp4, MP4_TRACK_AUDIO, 0, &g_callbacks, g_audioTrack);
	int aSize = sceMp4TrackSampleBufQueryMemSize(MP4_TRACK_AUDIO, 50, 695, 0x2000, 0x10000);
	void *aBuf = allocHigh(aSize);
	sceMp4TrackSampleBufConstruct(g_mp4, g_audioTrack, aBuf, aSize, 50, 695, 0x2000, 0x10000);
	void *audioEs = allocHigh(0x10000);
	sceMpegInitAu(&g_mpeg, audioEs, &g_audioAu);
	u32 audioInfo[16] = {0};
	sceMp4GetAacTrackInfoData(g_mp4, g_audioTrack, audioInfo);
	sceMp4AacDecodeInitResource(1);
	sceMp4AacDecodeInit(g_aac);

	STEP("audio track + aac set up");
	printf("tracks %d, video %d samples, audio %d samples at %d Hz, %d channels\n",
		(int)movieInfo[0], (int)videoInfo[2], (int)audioInfo[2], (int)audioInfo[3], (int)audioInfo[4]);

	// Start both tracks from their first sample.
	sceMp4TrackSampleBufFlush(g_mp4, g_videoTrack);
	sceMp4PutSampleNum(g_mp4, g_videoTrack, 0);
	sceMp4TrackSampleBufFlush(g_mp4, g_audioTrack);
	sceMp4PutSampleNum(g_mp4, g_audioTrack, 0);

	int i;
	for (i = 0; i < AUDIO_BUFS; i++) {
		g_audioBufs[i] = memalign(64, AUDIO_SAMPLES * 4);
		memset(g_audioBufs[i], 0, AUDIO_SAMPLES * 4);
	}
	g_frameBuf = allocHigh(512 * 272 * 4);
	for (i = 0; i < 4; i++) {
		g_stopBufs[i] = allocHigh(512 * 272 * 4);
	}

	STEP("buffers flushed, starting display");
	sceGuInit();
	sceGuStart(GU_DIRECT, g_list);
	sceGuDrawBuffer(GU_PSM_8888, (void *)0, 512);
	sceGuDispBuffer(480, 272, (void *)0x88000, 512);
	sceGuOffset(2048 - 240, 2048 - 136);
	sceGuViewport(2048, 2048, 480, 272);
	sceGuScissor(0, 0, 480, 272);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFinish();
	sceGuSync(0, 0);
	sceGuDisplay(GU_TRUE);

	sceAudioOutput2Reserve(AUDIO_SAMPLES);
	SceUID sound = sceKernelCreateThread("soundThread", soundThread, 0x1e, 0x1000, 0, NULL);
	sceKernelStartThread(sound, 0, NULL);

	STEP("sound thread started, entering loop");
	const int videoTotal = (int)videoInfo[2] < MAX_FRAMES ? (int)videoInfo[2] : MAX_FRAMES;
	const int audioTotal = (int)audioInfo[2];
	const int sampleRate = audioInfo[3] ? (int)audioInfo[3] : 44100;
	int framePending = 0;
	int ausDelivered = 0;
	int stopped = 0, flushLeft = 0, flushIndex = 0;
	int iterations = 0;
	// The track's duration is in 90 kHz units, like the PTS.
	const int frameDuration = videoInfo[2] ? (int)(videoInfo[1] / videoInfo[2]) : 3003;
	u32 framePts = 0;
	u32 start = sceKernelGetSystemTimeLow();

	// Each vblank: keep the buffers full, decode audio while there's room, decode the next video
	// frame if we don't have one waiting, and show it once the audio clock has reached its PTS.
	while (g_frames < videoTotal && (u32)(sceKernelGetSystemTimeLow() - start) < 20000000) {
		// Each log line is a round trip over USB, so keep them rare during playback.
		const int verbose = iterations == 0 || ausDelivered >= videoTotal || (iterations % 150) == 0;
		if (verbose) STEP("iter %d: frames %d aus %d audio %d/%d", iterations, g_frames, ausDelivered, g_audioDecoded, g_audioPlayed);
		iterations++;
		topUp(g_videoTrack);
		topUp(g_audioTrack);
		if (verbose) STEP("  topped up");

		// The output call returns once the previous block has finished, while the hardware still
		// plays the one just queued, so two buffers besides the queue are in use and mustn't be
		// decoded into yet.
		while (g_audioDecoded < audioTotal && g_audioDecoded - g_audioPlayed < AUDIO_BUFS - 2) {
			u32 tg = sceKernelGetSystemTimeLow();
			if (sceMp4GetAacAu(g_mp4, g_audioTrack, &g_audioAu, g_auInfo) != 0) {
				break;
			}
			u32 t0 = sceKernelGetSystemTimeLow();
			g_aacGetUs += t0 - tg;
			sceMp4AacDecode(g_aac, &g_audioAu, g_audioBufs[g_audioDecoded % AUDIO_BUFS], g_audioDecoded == 0, sampleRate);
			int dt = (int)(sceKernelGetSystemTimeLow() - t0);
			g_aacUs += dt;
			if (dt > g_aacMax) g_aacMax = dt;
			g_audioDecoded++;
		}

		if (!framePending) {
			SceInt32 status = 0;
			u32 t0 = sceKernelGetSystemTimeLow(), t1 = t0, t2;
			if (ausDelivered < videoTotal) {
				int r = sceMp4GetAvcAu(g_mp4, g_videoTrack, &g_videoAu, g_auInfo);
				if (verbose) STEP("  GetAvcAu %08x size %d pts %u dts %u", r, (int)g_videoAu.iAuSize, (unsigned)g_videoAu.iPts, (unsigned)g_videoAu.iDts);
				if (r == 0) {
					ausDelivered++;
					t1 = sceKernelGetSystemTimeLow();
					result = sceMpegAvcDecode(&g_mpeg, &g_videoAu, 512, &g_frameBuf, &status);
					if (verbose) STEP("  AvcDecode %08x status %d", result, (int)status);
					if (result != 0) {
						printf("sceMpegAvcDecode: %08x at frame %d\n", result, g_frames);
						break;
					}
				}
			} else if (!stopped) {
				// The decoder holds some pictures back. At the end of the stream, DecodeStop writes
				// all of them out at once, into an array of four frame buffers, and status says how
				// many there are. Then they're shown one per frame like the rest.
				t1 = sceKernelGetSystemTimeLow();
				if (verbose) STEP("  AvcDecodeStop");
				sceMpegAvcDecodeStop(&g_mpeg, 512, g_stopBufs, &status);
				if (verbose) STEP("  AvcDecodeStop status %d", (int)status);
				stopped = 1;
				flushLeft = status;
				flushIndex = 0;
				if (flushLeft == 0) {
					break;
				}
				status = 0;
			}
			if (stopped && flushLeft > 0) {
				// Point the blit at the next of the flushed pictures.
				g_frameBuf = g_stopBufs[flushIndex++];
				flushLeft--;
				status = 1;
				t1 = sceKernelGetSystemTimeLow();
			} else if (stopped) {
				break;
			}
			t2 = sceKernelGetSystemTimeLow();
			if (status != 0) {
				g_getAvcUs[g_frames] = t1 - t0;
				g_avcDecodeUs[g_frames] = t2 - t1;
				// Pictures come out in display order, so the n-th one is due at n frame durations.
				framePts = (u32)((long long)g_frames * frameDuration);
				framePending = 1;
				u32 t3 = sceKernelGetSystemTimeLow();
				present();
				g_blitUs[g_frames] = sceKernelGetSystemTimeLow() - t3;
			}
		}

		if (verbose) STEP("  waiting for vblank");
		sceDisplayWaitVblankStart();
		// The audio clock, in 90 kHz units like the PTS.
		u32 audioClock = (u32)((long long)g_audioPlayed * AUDIO_SAMPLES * 90000 / sampleRate);
		if (framePending && framePts <= audioClock) {
			flip();
			g_flipTime[g_frames++] = sceKernelGetSystemTimeLow();
			framePending = 0;
		}
	}
	STEP("loop done");
	u32 end = sceKernelGetSystemTimeLow();
	g_quit = 1;
	sceKernelWaitThreadEnd(sound, NULL);

	printf("frames %d of %d, audio %d decoded, %d played, %d us\n", g_frames, videoTotal, g_audioDecoded, g_audioPlayed, (int)(end - start));
	if (g_frames > 1) {
		int span = g_flipTime[g_frames - 1] - g_flipTime[0];
		printf("flip span %d us -> %d.%02d fps\n", span, (int)((g_frames - 1) * 1000000LL / span), (int)((g_frames - 1) * 100000000LL / span % 100));
	}
	if (g_frames > 1) {
		int hist[5] = {0}, sec;
		for (i = 1; i < g_frames; i++) {
			int vb = (int)((g_flipTime[i] - g_flipTime[i - 1] + 8342) / 16683);
			hist[vb > 4 ? 4 : vb]++;
		}
		printf("flip gaps: 1 vblank %d, 2 vblanks %d, 3 vblanks %d, more %d\n", hist[1], hist[2], hist[3], hist[4]);
		for (sec = 0; sec * 1000000 <= (int)(g_flipTime[g_frames - 1] - start); sec++) {
			int f = 0;
			for (i = 0; i < g_frames; i++) {
				int t = (int)(g_flipTime[i] - start);
				if (t >= sec * 1000000 && t < (sec + 1) * 1000000) f++;
			}
			printf("  sec %2d: %2d frames\n", sec, f);
		}
	}
	printTimes("sceMp4GetAvcAu", g_getAvcUs, g_frames);
	printTimes("sceMpegAvcDecode", g_avcDecodeUs, g_frames);
	printTimes("blit (15 strips)", g_blitUs, g_frames);
	printf("%-20s avg %6d us\n", "sceMp4GetAacAu", g_audioDecoded ? (int)(g_aacGetUs / g_audioDecoded) : 0);
	printf("%-20s avg %6d us, max %6d (%d AUs)\n", "sceMp4AacDecode", g_audioDecoded ? (int)(g_aacUs / g_audioDecoded) : 0, g_aacMax, g_audioDecoded);
	printf("%-20s avg %6d us, max %6d (%d calls)\n", "TrackSampleBufPut", g_puts ? (int)(g_putUs / g_puts) : 0, g_putMax, g_puts);

	sceGuTerm();
	sceAudioOutput2Release();
	sceMp4AacDecodeExit(g_aac);
	sceMp4AacDecodeTermResource();
	sceMp4TrackSampleBufDestruct(g_mp4, g_audioTrack);
	sceMp4UnregistTrack(g_mp4, g_audioTrack);
	sceMp4TrackSampleBufDestruct(g_mp4, g_videoTrack);
	sceMp4UnregistTrack(g_mp4, g_videoTrack);
	sceMp4Delete(g_mp4);
	sceMp4Finish();
	sceMpegDelete(&g_mpeg);
	sceMpegAvcResourceFinish();
	sceMpegFinish();
	return 0;
}
