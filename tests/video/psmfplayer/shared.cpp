#include "shared.h"

#include <psputility.h>
#include <psputility_modules.h>
#include <pspmodulemgr.h>

static unsigned int __attribute__((aligned(16))) list[262144];
static SceUID psmfModule;
static SceUID psmfPlayerModule;

int initVideo() {
	sceKernelDcacheWritebackAll();

	sceGuInit();
	sceGuStart(GU_DIRECT, list);

	sceGuDrawBuffer (GU_PSM_8888, (void*)FRAME_SIZE, BUF_WIDTH);
	sceGuDepthBuffer((void *)(FRAME_SIZE * 2), BUF_WIDTH);
	sceGuOffset     (2048 - (SCR_WIDTH / 2),2048 - (SCR_HEIGHT / 2));
	sceGuViewport   (2048, 2048, SCR_WIDTH, SCR_HEIGHT);
	sceGuDepthRange (0xc350, 0x2710);
	sceGuScissor    (0, 0, SCR_WIDTH, SCR_HEIGHT);
	sceGuFinish     ();
	sceGuSync       (GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);

	return 0;
}

int loadPsmfPlayer() {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_MPEGBASE);

	if (!RUNNING_ON_EMULATOR) {
		psmfModule = sceKernelLoadModule("psmf.prx", 0, NULL);
		if (psmfModule < 0) {
			printf("TEST FAILURE: Please place a psmf.prx in this directory.");
			return -1;
		}
		sceKernelStartModule(psmfModule, 0, NULL, NULL, NULL);

		psmfPlayerModule = sceKernelLoadModule("libpsmfplayer.prx", 0, NULL);
		if (psmfPlayerModule < 0) {
			printf("TEST FAILURE: Please place a libpsmfplayer.prx in this directory.");
			return -1;
		}

		sceKernelStartModule(psmfPlayerModule, 0, NULL, NULL, NULL);
	}
}

void unloadPsmfPlayer() {
	sceKernelStopModule(psmfPlayerModule, 0, NULL, NULL, NULL);
	sceKernelUnloadModule(psmfPlayerModule);
	sceKernelStopModule(psmfModule, 0, NULL, NULL, NULL);
	sceKernelUnloadModule(psmfModule);

	sceUtilityUnloadModule(PSP_MODULE_AV_MPEGBASE);
	sceUtilityUnloadModule(PSP_MODULE_AV_AVCODEC);
}
