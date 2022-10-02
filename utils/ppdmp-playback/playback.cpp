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
		}

		if (set_filename) {
			printf("Unexpected argument %s\n", argv[i]);
			printf("Usage: playback.prx filename [--start=1] [--end=1000]\n");
			return 1;
		}

		filename = argv[i];
		set_filename = true;
	}

	Replay replay(filename);
	replay.SetRange(start, end);
	printf("VALID: %d\n", replay.Valid());
	printf("RUN: %d\n", replay.Run());

	uint topaddr;
	int bufferwidth;
	int pixelformat;

	sceDisplayGetFrameBuf((void **)&topaddr, &bufferwidth, &pixelformat, 0);
	printf("SCREENSHOT: %08x, %d, %d\n", topaddr, bufferwidth, pixelformat);

	emulatorEmitScreenshot();

	sceGuTerm();

	return 0;
}