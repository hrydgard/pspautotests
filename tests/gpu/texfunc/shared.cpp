#include "shared.h"

u32 __attribute__((aligned(16))) list[262144];
static u32 copybuf[512 * 272];

GePtr fbp0 = 0;
GePtr dbp0 = fbp0 + 512 * 272 * sizeof(u32);

void initDisplay() { 
	sceGuInit();
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888, fbp0, 512);
	sceGuDispBuffer(480, 272, fbp0, 512);
	sceGuDepthBuffer(dbp0, 512);
	sceGuOffset(2048 - (480 / 2), 2048 - (272 / 2));
	sceGuViewport(2048, 2048, 480, 272);
	sceGuDepthRange(65535, 0);
	sceGuDepthMask(0);
	sceGuScissor(0, 0, 8, 8);
	sceGuDisable(GU_SCISSOR_TEST);
	sceGuFrontFace(GU_CW);
	sceGuShadeModel(GU_SMOOTH);
	sceGuEnable(GU_TEXTURE_2D);

	ScePspFMatrix4 ones = {
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{0, 0, 0, 1},
	};

	sceGuSetMatrix(GU_MODEL, &ones);
	sceGuSetMatrix(GU_VIEW, &ones);
	sceGuSetMatrix(GU_PROJECTION, &ones);

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
	copybuf[0] = c;
	sceKernelDcacheWritebackInvalidateRange(copybuf, sizeof(copybuf[0]));
	sceDmacMemcpy(sceGeEdramGetAddr(), copybuf, 512 * sizeof(copybuf[0]));
}

u32 readDispBuffer() {
	sceKernelDcacheWritebackInvalidateRange(copybuf, sizeof(copybuf[0]));
	sceDmacMemcpy(copybuf, sceGeEdramGetAddr(), 512 * sizeof(copybuf[0]));
	return copybuf[0];
}
