#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <psppower.h>
#include <stdio.h>
#include <string.h>
#include <common.h>
#include "replay.h"

// Just to make IntelliSense happy, not attempting to compile.
#ifdef _MSC_VER
#define __attribute__(...)
#endif

extern "C" int sceDmacMemcpy(void *dest, const void *source, unsigned int size);

// All of user memory but 1 MB (for thread stacks): common.c's 21 MB default is too small for big dumps.
PSP_HEAP_SIZE_KB(-1024);

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272

static unsigned int __attribute__((aligned(16))) list[1024];

void init() {
	void *fbp0 = 0;

	scePowerSetClockFrequency(333, 333, 166);
	memset((void *)0x04000000, 0, 0x00200000);
	sceKernelDcacheWritebackInvalidateAll();

	sceGuInit();
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888, fbp0, BUF_WIDTH);
	sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, fbp0, BUF_WIDTH);
	sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFinish();
	sceGuSync(0, 0);

	sceDisplayWaitVblankStart();
	sceGuDisplay(1);
}

extern int HAS_DISPLAY;

extern "C" int main(int argc, char *argv[]) {
	init();
	HAS_DISPLAY = 0;

	const char *filename = "host0:/framedump.ppdmp";
	bool set_filename = false;
	int start = 1;
	int end = 0x7FFFFFFF;
	int holdMs = 0;
	int progress = 0;
	int traceFrom = 0;
	int cmdLimit = 0;
	bool saveDepth = true;
	unsigned int displayAddr = 0;
	int displayStride = 512, displayFormat = 3;

	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (!strncmp(argv[i], "--start=", strlen("--start="))) {
				start = atoi(argv[i] + strlen("--start="));
				continue;
			}
			if (!strncmp(argv[i], "--end=", strlen("--end="))) {
				end = atoi(argv[i] + strlen("--end="));
				continue;
			}
			if (!strncmp(argv[i], "--display=", strlen("--display="))) {
				// addr,stride,format: show (and screenshot) this buffer instead of the dump's display.
				sscanf(argv[i] + strlen("--display="), "%x,%d,%d", &displayAddr, &displayStride, &displayFormat);
				continue;
			}
			if (!strcmp(argv[i], "--no-depth")) {
				saveDepth = false;
				continue;
			}
			if (!strncmp(argv[i], "--trace-from=", strlen("--trace-from="))) {
				traceFrom = atoi(argv[i] + strlen("--trace-from="));
				continue;
			}
			if (!strncmp(argv[i], "--cmds=", strlen("--cmds="))) {
				cmdLimit = atoi(argv[i] + strlen("--cmds="));
				continue;
			}
			if (!strncmp(argv[i], "--progress=", strlen("--progress="))) {
				progress = atoi(argv[i] + strlen("--progress="));
				continue;
			}
			if (!strncmp(argv[i], "--hold-ms=", strlen("--hold-ms="))) {
				holdMs = atoi(argv[i] + strlen("--hold-ms="));
				continue;
			}
		}

		if (set_filename) {
			printf("Unexpected argument %s\n", argv[i]);
			printf("Usage: playback.prx filename [--start=1] [--end=1000] [--hold-ms=1500]\n");
			return 1;
		}

		filename = argv[i];
		set_filename = true;
	}

	Replay replay(filename);
	replay.SetRange(start, end);
	replay.SetProgress(progress, traceFrom);
	replay.SetCommandLimit(cmdLimit);
	printf("VALID: %d\n", replay.Valid());
	printf("RUN: %d\n", replay.Run());
	replay.ShowResult();
	if (displayAddr != 0) {
		sceDisplaySetFrameBuf((void *)displayAddr, displayStride, displayFormat, PSP_DISPLAY_SETBUF_NEXTFRAME);
		sceDisplayWaitVblankStart();
		sceDisplayWaitVblankStart();
	}

	uint topaddr;
	int bufferwidth;
	int pixelformat;

	sceDisplayGetFrameBuf((void **)&topaddr, &bufferwidth, &pixelformat, 0);
	printf("SCREENSHOT: %08x, %d, %d\n", topaddr, bufferwidth, pixelformat);

	emulatorEmitScreenshot();
	if (saveDepth)
		printf("DEPTH: %d\n", replay.SaveDepth("host0:/__depth.bin") ? 1 : 0);

	// Keep the result on screen for a while: PSPLink clears the display when the program exits.
	if (holdMs > 0) {
		sceKernelDelayThread(holdMs * 1000);
	}

	sceGuTerm();

	return 0;
}