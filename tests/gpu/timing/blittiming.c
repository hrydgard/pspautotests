// Times full-screen textured blits on the GE, the way movie players draw a decoded frame, across
// the things that change the cost: texture format, where the texture lives, strip width,
// filtering, blending, and a clear first. Timing probe, not a pass/fail test.
//
// Motivation: two games' movie frame rates depend on how long this blit takes. Star Wars: Lethal
// Alliance draws 16 strips of 32 pixels from an 8888 texture in RAM; Ys I & II Chronicles clears
// and then draws one full-width sprite from a 565 texture in RAM with linear filtering.
#include <common.h>

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <psppower.h>
#include <pspaudio.h>
#include <psputility.h>
#include "../../audio/sascore/sascore.h"

#include <stdio.h>
#include <string.h>
#include <malloc.h>

// The test harness echoes stdout to the debug screen, which would draw over the blits.
extern unsigned int HAS_DISPLAY;

static unsigned int __attribute__((aligned(64))) g_list[64 * 1024];
static u8 *g_ramTex;

typedef struct {
	u16 u, v;
	s16 x, y, z;
} Vertex;

typedef struct {
	const char *name;
	int texFormat;  // GU_PSM_*
	int inVram;
	int stripWidth;  // 480 = one sprite
	int linear;
	int blend;
	int clear;  // GU_COLOR_BUFFER_BIT etc., or 1 for color only
	int draw;  // 0 = clear only
	int totalWidth;  // 0 = 480; 512 draws a last strip entirely outside the scissor
	int fbFormat;  // 0 = same as the texture (565 for the 16-bit ones)
} Config;

static const Config configs[] = {
	{ "8888 ram strips32 nearest (Lethal Alliance)", GU_PSM_8888, 0, 32, 0, 0, 0, 1 },
	{ "8888 ram strips32 nearest blend", GU_PSM_8888, 0, 32, 0, 1, 0, 1 },
	{ "8888 ram strips64 nearest", GU_PSM_8888, 0, 64, 0, 0, 0, 1 },
	{ "8888 ram strips128 nearest", GU_PSM_8888, 0, 128, 0, 0, 0, 1 },
	{ "8888 ram sprite480 nearest", GU_PSM_8888, 0, 480, 0, 0, 0, 1 },
	{ "8888 ram strips32 linear", GU_PSM_8888, 0, 32, 1, 0, 0, 1 },
	{ "8888 ram sprite480 linear", GU_PSM_8888, 0, 480, 1, 0, 0, 1 },
	{ "8888 vram strips32 nearest", GU_PSM_8888, 1, 32, 0, 0, 0, 1 },
	{ "8888 vram sprite480 nearest", GU_PSM_8888, 1, 480, 0, 0, 0, 1 },
	{ "565 ram strips32 nearest", GU_PSM_5650, 0, 32, 0, 0, 0, 1 },
	{ "565 ram strips64 nearest", GU_PSM_5650, 0, 64, 0, 0, 0, 1 },
	{ "565 ram sprite480 nearest", GU_PSM_5650, 0, 480, 0, 0, 0, 1 },
	{ "565 ram sprite480 linear", GU_PSM_5650, 0, 480, 1, 0, 0, 1 },
	{ "565 ram sprite480 linear + clear (Ys)", GU_PSM_5650, 0, 480, 1, 0, 1, 1 },
	{ "565 ram strips32 linear", GU_PSM_5650, 0, 32, 1, 0, 0, 1 },
	{ "565 vram sprite480 nearest", GU_PSM_5650, 1, 480, 0, 0, 0, 1 },
	{ "clear only", GU_PSM_5650, 0, 480, 0, 0, 1, 0 },
	// Where the texture cache stops coping: strip width sweep.
	{ "8888 ram strips16 nearest", GU_PSM_8888, 0, 16, 0, 0, 0, 1 },
	{ "8888 ram strips192 nearest", GU_PSM_8888, 0, 192, 0, 0, 0, 1 },
	{ "8888 ram strips256 nearest", GU_PSM_8888, 0, 256, 0, 0, 0, 1 },
	{ "8888 ram strips320 nearest", GU_PSM_8888, 0, 320, 0, 0, 0, 1 },
	{ "8888 ram strips384 nearest", GU_PSM_8888, 0, 384, 0, 0, 0, 1 },
	{ "565 ram strips128 nearest", GU_PSM_5650, 0, 128, 0, 0, 0, 1 },
	{ "565 ram strips192 nearest", GU_PSM_5650, 0, 192, 0, 0, 0, 1 },
	{ "565 ram strips256 nearest", GU_PSM_5650, 0, 256, 0, 0, 0, 1 },
	{ "565 ram strips384 nearest", GU_PSM_5650, 0, 384, 0, 0, 0, 1 },
	{ "4444 ram strips32 nearest", GU_PSM_4444, 0, 32, 0, 0, 0, 1 },
	{ "4444 ram sprite480 nearest", GU_PSM_4444, 0, 480, 0, 0, 0, 1 },
	{ "8888 vram strips128 nearest", GU_PSM_8888, 1, 128, 0, 0, 0, 1 },
	{ "8888 vram strips256 nearest", GU_PSM_8888, 1, 256, 0, 0, 0, 1 },
	{ "8888 ram strips144 nearest", GU_PSM_8888, 0, 144, 0, 0, 0, 1 },
	{ "8888 ram strips160 nearest", GU_PSM_8888, 0, 160, 0, 0, 0, 1 },
	{ "8888 ram strips32 to 512, last off-scissor", GU_PSM_8888, 0, 32, 0, 0, 0, 1, 512 },
	// The framebuffer's format against the texture's.
	{ "8888 ram strips32 -> 565 fb", GU_PSM_8888, 0, 32, 0, 0, 0, 1, 0, GU_PSM_5650 },
	{ "8888 ram sprite480 -> 565 fb", GU_PSM_8888, 0, 480, 0, 0, 0, 1, 0, GU_PSM_5650 },
	{ "565 ram strips32 -> 8888 fb", GU_PSM_5650, 0, 32, 0, 0, 0, 1, 0, GU_PSM_8888 },
	{ "565 ram sprite480 -> 8888 fb", GU_PSM_5650, 0, 480, 0, 0, 0, 1, 0, GU_PSM_8888 },
	{ "8888 vram strips32 -> 565 fb", GU_PSM_8888, 1, 32, 0, 0, 0, 1, 0, GU_PSM_5650 },
	{ "8888 vram sprite480 -> 565 fb", GU_PSM_8888, 1, 480, 0, 0, 0, 1, 0, GU_PSM_5650 },
	{ "565 vram strips32 -> 565 fb", GU_PSM_5650, 1, 32, 0, 0, 0, 1 },
	{ "565 vram sprite480 -> 8888 fb", GU_PSM_5650, 1, 480, 0, 0, 0, 1, 0, GU_PSM_8888 },
	{ "clear only -> 8888 fb", GU_PSM_8888, 0, 480, 0, 0, 1, 0 },
	// Clears on their own, by what they clear.
	{ "clear color, 565 fb", GU_PSM_5650, 0, 480, 0, 0, GU_COLOR_BUFFER_BIT, 0 },
	{ "clear color+depth, 565 fb", GU_PSM_5650, 0, 480, 0, 0, GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT, 0 },
	{ "clear color+depth+stencil, 565 fb", GU_PSM_5650, 0, 480, 0, 0, GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT | GU_STENCIL_BUFFER_BIT, 0 },
	{ "clear depth only, 565 fb", GU_PSM_5650, 0, 480, 0, 0, GU_DEPTH_BUFFER_BIT, 0 },
	{ "clear color, 5551 fb", GU_PSM_5551, 0, 480, 0, 0, GU_COLOR_BUFFER_BIT, 0, 0, GU_PSM_5551 },
	{ "clear color+stencil, 5551 fb", GU_PSM_5551, 0, 480, 0, 0, GU_COLOR_BUFFER_BIT | GU_STENCIL_BUFFER_BIT, 0, 0, GU_PSM_5551 },
	{ "clear color, 8888 fb", GU_PSM_8888, 0, 480, 0, 0, GU_COLOR_BUFFER_BIT, 0 },
	{ "clear color+depth, 8888 fb", GU_PSM_8888, 0, 480, 0, 0, GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT, 0 },
	{ "clear color+depth+stencil, 8888 fb", GU_PSM_8888, 0, 480, 0, 0, GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT | GU_STENCIL_BUFFER_BIT, 0 },
	{ "clear depth only, 8888 fb", GU_PSM_8888, 0, 480, 0, 0, GU_DEPTH_BUFFER_BIT, 0 },
};

#define REPEATS 8

// A sceSasCore thread mixing in the background, like a game playing music: the mix runs on the
// Media Engine and shares the bus with the GE.
static volatile int g_sasRun, g_sasQuit;
static SasCore g_sasCore __attribute__((aligned(64)));
static short g_sasPcm[4096] __attribute__((aligned(64)));
static short g_sasOut[3][512 * 2] __attribute__((aligned(64)));

static int sasThread(SceSize args, void *argp) {
	int ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, 512, PSP_AUDIO_FORMAT_STEREO);
	int buf = 0;
	while (!g_sasQuit) {
		if (!g_sasRun) {
			sceKernelDelayThread(1000);
			continue;
		}
		short *out = g_sasOut[buf];
		buf = (buf + 1) % 3;
		__sceSasCore(&g_sasCore, out);
		sceAudioOutputPannedBlocking(ch, 0, 0, out);
	}
	sceAudioChRelease(ch);
	return 0;
}

// Threads that poll with sceKernelDelayThread(10), the way the game's reader and sound threads do.
static volatile int g_pollRun;
static int pollThread(SceSize args, void *argp) {
	while (!g_sasQuit) {
		if (!g_pollRun) {
			sceKernelDelayThread(1000);
			continue;
		}
		sceKernelDelayThread(10);
	}
	return 0;
}

static void startSas() {
	int v, i;
	for (i = 0; i < 4096; i++) {
		g_sasPcm[i] = (short)(((i * 5) & 255) * 64 - 8192);
	}
	__sceSasInit(&g_sasCore, 512, 32, 0, 44100);
	for (v = 0; v < 8; v++) {
		__sceSasSetVoicePCM(&g_sasCore, v, g_sasPcm, 4096, 1);
		__sceSasSetPitch(&g_sasCore, v, 0x1000);
		__sceSasSetADSR(&g_sasCore, v, 15, 0x40000000, 0, 0x7FFFFFFF, 0);
		__sceSasSetKeyOn(&g_sasCore, v);
	}
	SceUID th = sceKernelCreateThread("sas", sasThread, 0x10, 0x2000, 0, NULL);
	sceKernelStartThread(th, 0, NULL);
	th = sceKernelCreateThread("poll1", pollThread, 0x3d, 0x1000, 0, NULL);
	sceKernelStartThread(th, 0, NULL);
	th = sceKernelCreateThread("poll2", pollThread, 0x3b, 0x1000, 0, NULL);
	sceKernelStartThread(th, 0, NULL);
}

static int runConfig(const Config *c, int fbFormat) {
	void *tex = c->inVram ? (void *)(0x04000000 + 0x110000) : (void *)g_ramTex;

	u32 t0 = sceKernelGetSystemTimeLow();
	sceGuStart(GU_DIRECT, g_list);
	sceGuDrawBufferList(fbFormat, (void *)0, 512);
	if (c->clear) {
		sceGuClearColor(0);
		sceGuClearDepth(0);
		sceGuClearStencil(0);
		sceGuClear(c->clear == 1 ? GU_COLOR_BUFFER_BIT : c->clear);
	}
	if (c->draw) {
		sceGuEnable(GU_TEXTURE_2D);
		sceGuTexMode(c->texFormat, 0, 0, 0);
		sceGuTexImage(0, 512, 512, 512, tex);
		sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
		sceGuTexFilter(c->linear ? GU_LINEAR : GU_NEAREST, GU_NEAREST);
		sceGuColor(0xFFFFFFFF);
		if (c->blend) {
			sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xFFFFFF, 0x000000);
			sceGuEnable(GU_BLEND);
		} else {
			sceGuDisable(GU_BLEND);
		}
		const int total = c->totalWidth ? c->totalWidth : 480;
		int x;
		for (x = 0; x < total; x += c->stripWidth) {
			int w = x + c->stripWidth > total ? total - x : c->stripWidth;
			Vertex *v = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));
			v[0].u = x; v[0].v = 0; v[0].x = x; v[0].y = 0; v[0].z = 0;
			v[1].u = x + w; v[1].v = 272; v[1].x = x + w; v[1].y = 272; v[1].z = 0;
			sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
		}
		sceGuDisable(GU_BLEND);
		sceGuDisable(GU_TEXTURE_2D);
	}
	sceGuFinish();
	sceGuSync(0, 0);
	return (int)(sceKernelGetSystemTimeLow() - t0);
}

int main(int argc, char *argv[]) {
	HAS_DISPLAY = 0;

	g_ramTex = memalign(64, 512 * 512 * 4);
	int i;
	// Any content: the GE's cost doesn't depend on the texel values.
	for (i = 0; i < 512 * 512 * 4; i++) {
		g_ramTex[i] = (u8)(i * 2654435761u >> 24);
	}
	sceKernelDcacheWritebackAll();
	memcpy((void *)(0x44000000 + 0x110000), g_ramTex, 512 * 272 * 4);

	sceGuInit();
	sceGuStart(GU_DIRECT, g_list);
	sceGuDrawBuffer(GU_PSM_8888, (void *)0, 512);
	sceGuDispBuffer(480, 272, (void *)0x88000, 512);
	// After the VRAM test texture at 0x110000-0x198000.
	sceGuDepthBuffer((void *)0x198000, 512);
	sceGuOffset(2048 - 240, 2048 - 136);
	sceGuViewport(2048, 2048, 480, 272);
	sceGuScissor(0, 0, 480, 272);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuDisable(GU_DEPTH_TEST);
	sceGuFinish();
	sceGuSync(0, 0);
	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);

	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_SASCORE);
	startSas();
	// At the default clocks, and at the highest a game can pick, then at the default with SAS (8
	// voices, 2.3ms of every 11.6ms on the Media Engine) mixing in the background.
	// Then with two threads polling with sceKernelDelayThread(10), then with both SAS and those.
	static const int clocks[5][3] = { { 222, 222, 111 }, { 333, 333, 166 }, { 222, 222, 111 }, { 222, 222, 111 }, { 222, 222, 111 } };
	int k, c;
	for (k = 0; k < 5; k++) {
	scePowerSetClockFrequency(clocks[k][0], clocks[k][1], clocks[k][2]);
	g_sasRun = k == 2 || k == 4;
	g_pollRun = k == 3 || k == 4;
	sceKernelDelayThread(50000);
	printf("clocks: cpu %d, bus %d%s%s\n", scePowerGetCpuClockFrequencyInt(), scePowerGetBusClockFrequencyInt(), g_sasRun ? ", with SAS mixing" : "", g_pollRun ? ", with polling threads" : "");
	printf("%-48s %8s %8s\n", "config (framebuffer same format as texture)", "min us", "avg us");
	for (c = 0; c < (int)(sizeof(configs) / sizeof(configs[0])); c++) {
		const Config *cfg = &configs[c];
		int fbFormat = cfg->fbFormat ? cfg->fbFormat : cfg->texFormat == GU_PSM_8888 ? GU_PSM_8888 : GU_PSM_5650;
		int minUs = 0x7FFFFFFF, sum = 0, r;
		runConfig(cfg, fbFormat);  // warm up
		for (r = 0; r < REPEATS; r++) {
			int us = runConfig(cfg, fbFormat);
			sum += us;
			if (us < minUs) minUs = us;
		}
		printf("%-48s %8d %8d\n", cfg->name, minUs, sum / REPEATS);
	}
	}
	g_sasQuit = 1;
	sceKernelDelayThread(100000);
	scePowerSetClockFrequency(222, 222, 111);

	sceGuTerm();
	return 0;
}
