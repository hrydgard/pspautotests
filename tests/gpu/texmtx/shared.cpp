#include "shared.h"

u32 __attribute__((aligned(16))) list[32768];
static u32 copybuf[512 * 272];
static bool copybufUpdated;

GePtr fbp0 = 0;
GePtr dbp0 = fbp0 + 512 * 272 * sizeof(u32);

void initDisplay() { 
	sceGuInit();
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888, fbp0, 512);
	sceGuDispBuffer(480, 272, fbp0, 512);
	sceGuDepthBuffer(dbp0, 512);
	sceGuOffset(2048, 2048);
	sceGuViewport(2048, 2048, 512, -512);
	sceGuDepthRange(65535, 0);
	sceGuDepthMask(GU_TRUE);
	sceGuScissor(0, 0, 256, 256);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFrontFace(GU_CW);
	sceGuShadeModel(GU_SMOOTH);
	sceGuEnable(GU_CLIP_PLANES);
	sceGuEnable(GU_TEXTURE_2D);
	sceGuDisable(GU_FRAGMENT_2X);
	sceGuDisable(GU_BLEND);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);

	ScePspFMatrix4 ones = {
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{0, 0, 0, 1},
	};

	sceGuSetMatrix(GU_MODEL, &ones);
	sceGuSetMatrix(GU_VIEW, &ones);
	sceGuSetMatrix(GU_PROJECTION, &ones);
	// Use an identity by default.
	sceGuSetMatrix(GU_TEXTURE, &ones);

	sceGuTexFilter(GU_NEAREST, GU_NEAREST);
	sceGuTexMode(GU_PSM_8888, 0, 0, 0);

	sceGuFinish();
	sceGuSync(GU_SYNC_WAIT, GU_SYNC_WHAT_DONE);
 
	sceDisplayWaitVblankStart();
	sceGuDisplay(1);

	sceDisplaySetFrameBuf(sceGeEdramGetAddr(), 512, GU_PSM_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
	sceDisplaySetMode(0, 480, 272);
}

void startFrame() {
	sceGuStart(GU_DIRECT, list);
}

void endFrame() {
	sceGuFinish();
	sceGuSync(GU_SYNC_WAIT, GU_SYNC_WHAT_DONE);
}

void clearDispBuffer(u32 c) {
	for (int i = 0; i < 512 * 272; ++i)
		copybuf[i] = c;
	sceKernelDcacheWritebackInvalidateRange(copybuf, sizeof(copybuf));
	sceDmacMemcpy(sceGeEdramGetAddr(), copybuf, sizeof(copybuf));
	copybufUpdated = true;
}

u32 readDispBuffer(int x, int y) {
	if (!copybufUpdated) {
		sceKernelDcacheWritebackInvalidateRange(copybuf, sizeof(copybuf));
		sceDmacMemcpy(copybuf, sceGeEdramGetAddr(), sizeof(copybuf));
		copybufUpdated = true;
	}
	return copybuf[y * 512 + x];
}

void dirtyDispBuffer() {
	copybufUpdated = false;
}
