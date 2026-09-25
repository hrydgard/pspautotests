// A copy of the movie player in Star Wars: Lethal Alliance's loader (PSPLoader), instrumented to
// measure what paces its video on hardware. Timing probe, not a pass/fail test.
//
// The game: user_main (prio 0x20) loops { ctrl read; GetAvcAu+AvcDecode, and if a picture came out,
// GE blit + one WaitVblankStart + flip; delay 10; GetAtracAu+AtracDecode while fewer than 10
// buffers are queued; delay 10 }. readThread (0x3d) tops up a 170-packet ringbuffer, soundThread
// (0x3b) outputs once 9 buffers are queued. No clock or timestamp is consulted anywhere.
//
// Needs the game's movie next to it, which can't be included here: get it with
//   PPSSPPHeadless --dump-file disc0:/PSP_GAME/USRDIR/DATAPSP/MOVIES/LOGO.PMF --dump-file-out LOGO.PMF <iso>
// It streams the file from ms0: through sceIoRead the way the game streams from disc0:, and
// g_split selects sceMpegAvcDecodeYCbCr + sceMpegAvcCsc instead of the game's sceMpegAvcDecode.
// Findings: ppsspp-re docs/lethal-alliance-video-pacing.md.
#include "shared.h"

#define RING_PACKETS 170
#define AUDIO_BUFS 10
#define MAX_FRAMES 1000

static u8 *g_file;
static int g_fileSize;
static int g_streamOffset2;
static int g_streamSize;
static SceUID g_streamFd = -1;
static long long g_putUs;
static int g_putMax, g_puts, g_putPackets;
static int g_filePos;       // relative to the stream start, like ctx[0x1C]
static volatile int g_quit;
static SceUID g_readSema;

static void *g_audioBufs[AUDIO_BUFS];
static volatile int g_audioDecoded;   // ctx[0xE0]
static volatile int g_audioOutput;    // ctx[0xE4]

static int g_decodeUs[MAX_FRAMES];
static int g_getAuUs[MAX_FRAMES];
static u32 g_flipTime[MAX_FRAMES];
static int g_auSize[MAX_FRAMES];
static u32 g_audioOutTime[MAX_FRAMES];
static int g_frames;
static int g_audioOuts;
static int g_videoNoData;
static int g_iterations;

static unsigned int __attribute__((aligned(64))) g_list[64 * 1024];

static SceInt32 memCallback(void *data, SceInt32 numPackets, void *arg) {
	int bytes = numPackets * 2048;
	int left = g_fileSize - (g_streamOffset2 + g_filePos);
	if (bytes > left) {
		bytes = left;
	}
	if (bytes <= 0) {
		return 0;
	}
	if (g_streamFd >= 0) {
		// Like the game: a plain sceIoRead into the ringbuffer.
		int r = sceIoRead(g_streamFd, data, bytes);
		return r > 0 ? r / 2048 : 0;
	}
	memcpy(data, g_file + g_streamOffset2 + g_filePos, bytes);
	return bytes / 2048;
}

static int readThread(SceSize args, void *argp) {
	while (!g_quit) {
		int avail = sceMpegRingbufferAvailableSize((SceMpegRingbuffer *) &g_ringbuffer);
		if (avail > 0) {
			int n = avail > 32 ? 32 : avail;
			if (g_filePos + n * 2048 > g_streamSize) {
				n = (g_streamSize - g_filePos) / 2048;
			}
			if (n > 0) {
				u32 p0 = sceKernelGetSystemTimeLow();
				int got = sceMpegRingbufferPut((SceMpegRingbuffer *) &g_ringbuffer, n, avail);
				int pd = (int)(sceKernelGetSystemTimeLow() - p0);
				g_putUs += pd;
				if (pd > g_putMax) g_putMax = pd;
				g_puts++;
				if (got > 0) {
					g_filePos += got * 2048;
					g_putPackets += got;
				}
			}
		}
		sceKernelSignalSema(g_readSema, 1);
		sceKernelDelayThread(10);
	}
	return 0;
}

static int soundThread(SceSize args, void *argp) {
	while (!g_quit) {
		if (g_audioDecoded >= 9 && g_audioOutput < g_audioDecoded) {
			void *buf = g_audioBufs[g_audioOutput % AUDIO_BUFS];
			g_audioOutput++;
			sceAudioOutput2OutputBlocking(0x8000, buf);
			if (g_audioOuts < MAX_FRAMES) {
				g_audioOutTime[g_audioOuts++] = sceKernelGetSystemTimeLow();
			}
		}
		sceKernelDelayThread(10);
	}
	return 0;
}

static const char *g_stepNames[] = {"ctrl", "getavcau", "avcdecode", "ge", "vblank", "setfb", "delay1", "atrac", "delay2", "powertick"};
#define NUM_STEPS 10
static long long g_stepUs[NUM_STEPS];
static int g_stepMax[NUM_STEPS];
static u32 g_mark;
static void lap(int step) {
	u32 t = sceKernelGetSystemTimeLow();
	int d = (int)(t - g_mark);
	g_stepUs[step] += d;
	if (d > g_stepMax[step]) g_stepMax[step] = d;
	g_mark = t;
}

// 1: decode with sceMpegAvcDecodeYCbCr + sceMpegAvcCsc, timed separately, instead of sceMpegAvcDecode.
static int g_split = 1;
static void *g_ycbcr;
static int g_cscUs[MAX_FRAMES];
extern int sceMpegAvcQueryYCbCrSize(SceMpeg *mpeg, int mode, int width, int height, int *result);
extern int sceMpegAvcInitYCbCr(SceMpeg *mpeg, int mode, int width, int height, void *ycbcr);
extern int sceMpegAvcDecodeYCbCr(SceMpeg *mpeg, SceMpegAu *au, void **buffer, SceInt32 *init);
extern int sceMpegAvcCsc(SceMpeg *mpeg, void *source, int *range, int frameWidth, void *dest);

static void *g_frameBuf;
static int g_drawBuf;

static void present() {
	// The game's blit: one 8888 textured pass from the decode buffer into the back buffer.
	sceGuStart(GU_DIRECT, g_list);
	sceGuDrawBufferList(GU_PSM_8888, (void *)(g_drawBuf ? 0x88000 : 0), 512);
	sceGuTexMode(GU_PSM_8888, 0, 0, 0);
	sceGuTexImage(0, 512, 512, 512, g_frameBuf);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
	sceGuTexFilter(GU_NEAREST, GU_NEAREST);
	sceGuEnable(GU_TEXTURE_2D);
	sceGuDisable(GU_DEPTH_TEST);
	// The game's state: blending on with fixed factors that amount to a replace.
	sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xFFFFFF, 0x000000);
	sceGuEnable(GU_BLEND);
	typedef struct { u16 u, v; s16 x, y, z; } V;
	// Like the game, 16 strips 32 pixels wide, so each stays inside the texture cache.
	int strip;
	for (strip = 0; strip < 16; strip++) {
		V *v = (V *)sceGuGetMemory(2 * sizeof(V));
		v[0].u = strip * 32; v[0].v = 0; v[0].x = strip * 32; v[0].y = 0; v[0].z = 0;
		v[1].u = strip * 32 + 32; v[1].v = 272; v[1].x = strip * 32 + 32; v[1].y = 272; v[1].z = 0;
		sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
	}
	sceGuFinish();
	sceGuSync(0, 0);
	lap(3);
	sceDisplayWaitVblankStart();
	lap(4);
	sceDisplaySetFrameBuf((void *)(0x04000000 + (g_drawBuf ? 0x88000 : 0)), 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
	lap(5);
	g_drawBuf ^= 1;
}

static int playerThread(SceSize args, void *argp) {
	SceCtrlData pad;
	int initAudio = 1;
	int idle = 0;

	u32 begin = sceKernelGetSystemTimeLow();
	while (g_frames < MAX_FRAMES && (u32)(sceKernelGetSystemTimeLow() - begin) < 12000000) {
		g_mark = sceKernelGetSystemTimeLow();
		sceCtrlReadBufferPositive(&pad, 1);
		lap(0);

		// Video, as in the game's 08807d04.
		SceInt32 attr = 0;
		u32 t0 = sceKernelGetSystemTimeLow();
		int result = sceMpegGetAvcAu(&g_mpeg, g_avc_stream, &g_avc_au, &attr);
		u32 t1 = sceKernelGetSystemTimeLow();
		lap(1);
		if (result == 0) {
			idle = 0;
			SceInt32 status = 0;
			u32 t2, t3 = 0;
			if (g_split) {
				result = sceMpegAvcDecodeYCbCr(&g_mpeg, &g_avc_au, &g_ycbcr, &status);
				t2 = sceKernelGetSystemTimeLow();
				if (result == 0 && status != 0) {
					int range[4] = { 0, 0, 480, 272 };
					int r = sceMpegAvcCsc(&g_mpeg, g_ycbcr, range, 512, g_frameBuf);
					if (r != 0 && g_frames < 3) {
						printf("sceMpegAvcCsc: %08x\n", r);
					}
				}
				t3 = sceKernelGetSystemTimeLow();
			} else {
				result = sceMpegAvcDecode(&g_mpeg, &g_avc_au, 0, &g_frameBuf, &status);
				t2 = sceKernelGetSystemTimeLow();
			}
			lap(2);
			if (result != 0) {
				printf("sceMpegAvcDecode: %08x\n", result);
				break;
			}
			if (status != 0) {
				g_getAuUs[g_frames] = t1 - t0;
				g_decodeUs[g_frames] = t2 - t1;
				g_cscUs[g_frames] = g_split ? (int)(t3 - t2) : 0;
				g_auSize[g_frames] = g_avc_au.iAuSize;
				present();
				g_flipTime[g_frames] = sceKernelGetSystemTimeLow();
				g_frames++;
			}
		} else {
			g_videoNoData++;
			if (g_filePos >= g_streamSize && ++idle > 120) {
				break;
			}
		}
		g_mark = sceKernelGetSystemTimeLow();
		sceKernelDelayThread(10);
		lap(6);

		// Audio, as in the game's 08807e28.
		if (g_audioDecoded - g_audioOutput < AUDIO_BUFS) {
			void *es;
			if (sceMpegGetAtracAu(&g_mpeg, g_atrac_stream, &g_atrac_au, &es) == 0) {
				sceMpegAtracDecode(&g_mpeg, &g_atrac_au, g_audioBufs[g_audioDecoded % AUDIO_BUFS], initAudio);
				initAudio = 0;
				g_audioDecoded++;
			}
		}
		lap(7);
		sceKernelDelayThread(10);
		lap(8);
		scePowerTick(0);
		lap(9);
		g_iterations++;
	}
	return 0;
}

int main(int argc, char *argv[]) {
	if (loadVideoModules() < 0) {
		return 1;
	}

	printf("clocks before: cpu %d bus %d\n", scePowerGetCpuClockFrequencyInt(), scePowerGetBusClockFrequencyInt());
	scePowerSetClockFrequency(222, 222, 111);
	printf("clocks now: cpu %d bus %d\n", scePowerGetCpuClockFrequencyInt(), scePowerGetBusClockFrequencyInt());
	SceUID fd = sceIoOpen("LOGO.PMF", PSP_O_RDONLY, 0777);
	if (fd < 0) {
		printf("Can't open LOGO.PMF: %08x\n", fd);
		return 1;
	}
	g_fileSize = sceIoLseek32(fd, 0, SEEK_END);
	sceIoLseek32(fd, 0, SEEK_SET);
	g_file = memalign(64, g_fileSize);
	int readBytes = 0;
	while (readBytes < g_fileSize) {
		int r = sceIoRead(fd, g_file + readBytes, g_fileSize - readBytes);
		if (r <= 0) {
			break;
		}
		readBytes += r;
	}
	sceIoClose(fd);
	printf("file %d bytes, read %d\n", g_fileSize, readBytes);

	// Stream from the memory stick the way the game streams from disc0:.
	SceUID out = sceIoOpen("ms0:/playertiming.pmf", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	sceIoWrite(out, g_file, g_fileSize);
	sceIoClose(out);
	g_streamFd = sceIoOpen("ms0:/playertiming.pmf", PSP_O_RDONLY, 0777);
	printf("streaming from ms0: %s\n", g_streamFd >= 0 ? "yes" : "no");

	sceMpegInit();
	int ringSize = sceMpegRingbufferQueryMemSize(RING_PACKETS);
	int mpegSize = sceMpegQueryMemSize(0);
	void *ringData = memalign(64, ringSize);
	void *mpegData = memalign(64, mpegSize);
	sceMpegRingbufferConstruct((SceMpegRingbuffer *) &g_ringbuffer, RING_PACKETS, ringData, ringSize, &memCallback, NULL);
	int result = sceMpegCreate(&g_mpeg, mpegData, mpegSize, (SceMpegRingbuffer *) &g_ringbuffer, 512, 0, 0);
	if (result != 0) {
		printf("sceMpegCreate: %08x\n", result);
		return 1;
	}
	sceMpegQueryStreamOffset(&g_mpeg, g_file, (SceInt32 *)&g_streamOffset2);
	sceMpegQueryStreamSize(g_file, (SceInt32 *)&g_streamSize);
	if (g_streamFd >= 0) {
		sceIoLseek32(g_streamFd, g_streamOffset2, SEEK_SET);
	}

	g_avc_stream = sceMpegRegistStream(&g_mpeg, 0, 0);
	g_atrac_stream = sceMpegRegistStream(&g_mpeg, 1, 0);
	g_avc_buf = sceMpegMallocAvcEsBuf(&g_mpeg);
	sceMpegInitAu(&g_mpeg, g_avc_buf, &g_avc_au);
	SceInt32 esSize, outSize;
	sceMpegQueryAtracEsSize(&g_mpeg, &esSize, &outSize);
	g_atracData = memalign(64, esSize);
	sceMpegInitAu(&g_mpeg, g_atracData, &g_atrac_au);
	int i;
	for (i = 0; i < AUDIO_BUFS; i++) {
		g_audioBufs[i] = memalign(64, outSize);
		memset(g_audioBufs[i], 0, outSize);
	}
	g_frameBuf = memalign(64, 512 * 272 * 4);
	if (g_split) {
		int ysize = 0;
		int r = sceMpegAvcQueryYCbCrSize(&g_mpeg, 1, 480, 272, &ysize);
		g_ycbcr = memalign(64, ysize);
		int r2 = sceMpegAvcInitYCbCr(&g_mpeg, 1, 480, 272, g_ycbcr);
		printf("split mode: QueryYCbCrSize %08x size %d, InitYCbCr %08x\n", r, ysize, r2);
	}

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

	sceCtrlSetSamplingCycle(0);
	sceAudioOutput2Reserve(2048);
	g_readSema = sceKernelCreateSema("ringbuf_sema", 0, 0, 1, NULL);

	// The game fills the ringbuffer before starting (the loop at 08808de8).
	while (sceMpegRingbufferAvailableSize((SceMpegRingbuffer *) &g_ringbuffer) > 0 && g_filePos < g_streamSize) {
		int avail = sceMpegRingbufferAvailableSize((SceMpegRingbuffer *) &g_ringbuffer);
		int n = avail > 32 ? 32 : avail;
		int got = sceMpegRingbufferPut((SceMpegRingbuffer *) &g_ringbuffer, n, avail);
		if (got <= 0) {
			break;
		}
		g_filePos += got * 2048;
	}

	SceUID reader = sceKernelCreateThread("readThread", readThread, 0x3d, 0x1000, 0, NULL);
	SceUID sound = sceKernelCreateThread("soundThread", soundThread, 0x3b, 0x1000, 0, NULL);
	SceUID player = sceKernelCreateThread("user_main", playerThread, 0x20, 0x4000, 0, NULL);
	sceKernelStartThread(sound, 0, NULL);
	sceKernelStartThread(reader, 0, NULL);
	u32 start = sceKernelGetSystemTimeLow();
	sceKernelStartThread(player, 0, NULL);
	sceKernelWaitThreadEnd(player, NULL);
	u32 end = sceKernelGetSystemTimeLow();
	g_quit = 1;
	sceKernelWaitThreadEnd(reader, NULL);
	sceKernelWaitThreadEnd(sound, NULL);

	for (i = 0; i < NUM_STEPS; i++) {
		printf("step %-10s total %9d us, avg %6d, max %6d\n", g_stepNames[i], (int)g_stepUs[i], g_iterations ? (int)(g_stepUs[i] / g_iterations) : 0, g_stepMax[i]);
	}
	printf("iterations %d\n", g_iterations);
	printf("puts %d (%d packets during play), total %d us, max %d\n", g_puts, g_putPackets, (int)g_putUs, g_putMax);
	printf("stream %d bytes, ring %d packets\n", g_streamSize, RING_PACKETS);
	printf("frames %d, video NO_DATA %d, audio decoded %d, output %d, run %d us\n", g_frames, g_videoNoData, g_audioDecoded, g_audioOutput, (int)(end - start));
	if (g_frames > 1) {
		int span = g_flipTime[g_frames - 1] - g_flipTime[0];
		printf("flip span %d us -> %d.%02d fps\n", span, (int)((g_frames - 1) * 1000000LL / span), (int)((g_frames - 1) * 100000000LL / span % 100));
		int minD = 0x7FFFFFFF, maxD = 0;
		long long sum = 0;
		for (i = 0; i < g_frames; i++) {
			sum += g_decodeUs[i];
			if (g_decodeUs[i] < minD) minD = g_decodeUs[i];
			if (g_decodeUs[i] > maxD) maxD = g_decodeUs[i];
		}
		printf("%s: avg %d us, min %d, max %d\n", g_split ? "sceMpegAvcDecodeYCbCr" : "sceMpegAvcDecode", (int)(sum / g_frames), minD, maxD);
		if (g_split) {
			long long csum = 0;
			int cmin = 0x7FFFFFFF, cmax = 0;
			for (i = 0; i < g_frames; i++) {
				csum += g_cscUs[i];
				if (g_cscUs[i] < cmin) cmin = g_cscUs[i];
				if (g_cscUs[i] > cmax) cmax = g_cscUs[i];
			}
			printf("sceMpegAvcCsc: avg %d us, min %d, max %d\n", (int)(csum / g_frames), cmin, cmax);
		}
		int hist[8] = {0};
		for (i = 1; i < g_frames; i++) {
			int vb = (int)((g_flipTime[i] - g_flipTime[i - 1] + 8342) / 16683);
			if (vb > 7) vb = 7;
			hist[vb]++;
		}
		for (i = 0; i < 8; i++) {
			printf("  flip gap %d vblanks: %d\n", i, hist[i]);
		}
		// Per second: frames flipped vs audio buffers output.
		int sec;
		for (sec = 0; sec < 20; sec++) {
			int f = 0, a = 0;
			for (i = 0; i < g_frames; i++) {
				int t = (int)(g_flipTime[i] - start);
				if (t >= sec * 1000000 && t < (sec + 1) * 1000000) f++;
			}
			for (i = 0; i < g_audioOuts; i++) {
				int t = (int)(g_audioOutTime[i] - start);
				if (t >= sec * 1000000 && t < (sec + 1) * 1000000) a++;
			}
			if (f || a) {
				printf("  sec %2d: video %2d, audio %2d\n", sec, f, a);
			}
		}
		for (i = 0; i < g_frames; i++) {
			printf("f %3d: t %8d getau %5d decode %6d size %5d\n", i, (int)(g_flipTime[i] - start), g_getAuUs[i], g_decodeUs[i], g_auSize[i]);
		}
	}

	if (g_streamFd >= 0) {
		sceIoClose(g_streamFd);
		sceIoRemove("ms0:/playertiming.pmf");
	}
	sceGuTerm();
	sceAudioOutput2Release();
	sceMpegDelete(&g_mpeg);
	sceMpegRingbufferDestruct((SceMpegRingbuffer *) &g_ringbuffer);
	unloadVideoModules();
	return 0;
}
