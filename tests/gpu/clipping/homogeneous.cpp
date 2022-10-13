#include <common.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include "../commands/commands.h"

extern "C" int sceDmacMemcpy(void *dest, const void *source, unsigned int size);
extern "C" int HAS_DISPLAY;

typedef struct {
	float x, y, z;
} VertexF32;

static const int BUF_PIXELS = 512 * 272;
static const int BUF_BYTES = BUF_PIXELS * sizeof(u32);

static u8 *fbp0 = 0;
static u8 *dbp0 = fbp0 + BUF_BYTES;

static u32 *copybuf = NULL;
static unsigned int __attribute__((aligned(16))) list[32768];

void init() {
	copybuf = new u32[512 * 272];

	sceGuInit();
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888, fbp0, 512);
	sceGuDispBuffer(480, 272, fbp0, 512);
	sceGuDepthBuffer(dbp0, 512);
	sceGuOffset(2048 - (512 / 2), 2048 - (256 / 2));
	sceGuViewport(2048, 2048, 256, -128);
	sceGuDepthRange(0, 65535);
	sceGuDepthMask(0);
	sceGuScissor(0, 0, 512, 272);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFrontFace(GU_CW);
	sceGuShadeModel(GU_SMOOTH);
	sceGuDisable(GU_TEXTURE_2D);
	sceGuDisable(GU_BLEND);
	sceGuAmbientColor(0xFFFFFFFF);
	sceGuAmbient(0xFFFFFFFF);

	sceGuSendCommandf(GE_CMD_VIEWPORTZ1, 32767.5);
	sceGuSendCommandf(GE_CMD_VIEWPORTZ2, 32767.5);
	sceGuSendCommandi(GE_CMD_MINZ, 0);
	sceGuSendCommandi(GE_CMD_MAXZ, 65535);
	sceGuSendCommandi(GE_CMD_REGION2, ((272 - 1) << 10) | (512 - 1));

	ScePspFMatrix4 ones = {
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{0, 0, 0, 1},
	};

	sceGuSetMatrix(GU_MODEL, &ones);
	sceGuSetMatrix(GU_VIEW, &ones);
	sceGuSetMatrix(GU_PROJECTION, &ones);

	sceGuFinish();
	sceGuSync(0, 0);

	sceDisplayWaitVblankStart();
	sceGuDisplay(1);
	sceDisplaySetFrameBuf(sceGeEdramGetAddr(), 512, 3, 1);
	sceDisplaySetFrameBuf(sceGeEdramGetAddr(), 512, 3, 0);

	HAS_DISPLAY = 0;
}

enum Side {
	SIDE_TOP,
	SIDE_RIGHT,
};

static void drawTR(const char *title, Side side, float xf, float yf, float znf, float zff, bool clamp = true) {
	memset(copybuf, 0, 512 * 272 * 4);
	sceKernelDcacheWritebackInvalidateRange(copybuf, BUF_BYTES);
	sceDmacMemcpy(sceGeEdramGetAddr(), copybuf, BUF_BYTES);
	sceKernelDcacheWritebackInvalidateRange(sceGeEdramGetAddr(), BUF_BYTES);

	sceGuStart(GU_DIRECT, list);
	// Actually depth clamp, but allows seeing verts outside Z.
	sceGuSetStatus(GU_CLIP_PLANES, clamp);

	VertexF32 verts[3] = {
		{ -1.0f * xf, -1.0f * yf, -1.0f * znf },
		{ 1.0f * xf, -1.0f * yf, 1.0f * zff },
		{ 1.0f * xf, 1.0f * yf, 1.0f * zff },
	};
	sceKernelDcacheWritebackInvalidateRange(verts, sizeof(verts));

	sceGuDrawArray(GU_TRIANGLES, GU_VERTEX_32BITF | GU_TRANSFORM_3D, 3, NULL, verts);

	sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);

	sceDmacMemcpy(copybuf, sceGeEdramGetAddr(), BUF_BYTES);
	sceKernelDcacheWritebackInvalidateRange(sceGeEdramGetAddr(), BUF_BYTES);

	int px = 0;
	if (side == SIDE_TOP) {
		int yoff = 128 - (int)(64 * yf);
		for (int i = 0; i < 512; ++i) {
			if (copybuf[512 * yoff + i] != 0) {
				++px;
			}
		}
	} else if (side == SIDE_RIGHT) {
		int xoff = 255 + (int)(128 * xf);
		for (int i = 0; i < 512; ++i) {
			if (copybuf[512 * i + xoff] != 0) {
				++px;
			}
		}
	}

	checkpoint("%s: %d", title, px);
}

static void drawWithW(const char *title, float w, bool clamp = true) {
	memset(copybuf, 0, 512 * 272 * 4);
	sceKernelDcacheWritebackInvalidateRange(copybuf, BUF_BYTES);
	sceDmacMemcpy(sceGeEdramGetAddr(), copybuf, BUF_BYTES);
	sceKernelDcacheWritebackInvalidateRange(sceGeEdramGetAddr(), BUF_BYTES);

	sceGuStart(GU_DIRECT, list);
	// Actually depth clamp, but allows seeing verts outside Z.
	sceGuSetStatus(GU_CLIP_PLANES, clamp);

	ScePspFMatrix4 onesWithW = {
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{0, 0, 0, w},
	};
	sceGuSetMatrix(GU_PROJECTION, &onesWithW);

	// We use flat Z just to avoid problems with Z clip.
	// We're also trying to make sure z + w is positive, so we can see if W clipped.
	float z = w >= 0.0f ? -w : 1.0f / -w;
	VertexF32 verts[3] = {
		{ -1.0f * w, -1.0f * w, z },
		{ 1.0f * w, -1.0f * w, -w + 0.5f },
		{ 1.0f * w, 1.0f * w, -w + 0.5f },
	};
	sceKernelDcacheWritebackInvalidateRange(verts, sizeof(verts));

	sceGuDrawArray(GU_TRIANGLES, GU_VERTEX_32BITF | GU_TRANSFORM_3D, 3, NULL, verts);

	sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);

	sceDmacMemcpy(copybuf, sceGeEdramGetAddr(), BUF_BYTES);
	sceKernelDcacheWritebackInvalidateRange(sceGeEdramGetAddr(), BUF_BYTES);

	int px = 0;
	for (int i = 0; i < 512; ++i) {
		if (copybuf[512 * 64 + i] != 0) {
			++px;
		}
	}

	checkpoint("%s: %d", title, px);
}

static void drawLinearW(const char *title, float w0, float w1, float w2) {
	memset(copybuf, 0, 512 * 272 * 4);
	sceKernelDcacheWritebackInvalidateRange(copybuf, BUF_BYTES);
	sceDmacMemcpy(sceGeEdramGetAddr(), copybuf, BUF_BYTES);
	sceKernelDcacheWritebackInvalidateRange(sceGeEdramGetAddr(), BUF_BYTES);

	sceGuStart(GU_DIRECT, list);
	// This one is trouble without clamp, so let's always enable.
	sceGuEnable(GU_CLIP_PLANES);

	ScePspFMatrix4 zAsW = {
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, -1, 1},
		{0, 0, 0, 0},
	};
	sceGuSetMatrix(GU_PROJECTION, &zAsW);

	VertexF32 verts[3] = {
		{ -1.0f * w0, -1.0f * w0, w0 },
		{ 1.0f * w1, -1.0f * w1, w1 },
		{ 1.0f * w2, 1.0f * w2, w2 },
	};
	sceKernelDcacheWritebackInvalidateRange(verts, sizeof(verts));

	sceGuDrawArray(GU_TRIANGLES, GU_VERTEX_32BITF | GU_TRANSFORM_3D, 3, NULL, verts);

	sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);

	sceDmacMemcpy(copybuf, sceGeEdramGetAddr(), BUF_BYTES);
	sceKernelDcacheWritebackInvalidateRange(sceGeEdramGetAddr(), BUF_BYTES);
	sceKernelDcacheWritebackInvalidateRange(copybuf, BUF_BYTES);

	int px = 0, x0 = -1, y0 = -1;
	for (int y = 0; y < 272; ++y) {
		for (int x = 0; x < 512; ++x) {
			if (copybuf[512 * y + x] != 0) {
				if (x0 == -1) {
					x0 = x;
				}
				if (y0 == -1) {
					y0 = y;
				}
				++px;
			}
		}
	}

	checkpoint("%s: %d (%d,%d)", title, px, x0, y0);
}

extern "C" int main(int argc, char *argv[]) {
	init();

	checkpointNext("NDC clip space (top):");
	drawTR("  All inside (top)", SIDE_TOP, 1.0f, 1.0f, 1.0f, 1.0f);
	drawTR("  X outside (top)", SIDE_TOP, 2.0f, 1.0f, 1.0f, 1.0f);
	drawTR("  Y outside (top)", SIDE_TOP, 1.0f, 2.0f, 1.0f, 1.0f);
	drawTR("  Z outside near (top)", SIDE_TOP, 1.0f, 1.0f, 2.0f, 1.0f);
	drawTR("  Z outside far (top)", SIDE_TOP, 1.0f, 1.0f, 1.0f, 2.0f);
	drawTR("  Z outside both (top)", SIDE_TOP, 1.0f, 1.0f, 2.0f, 2.0f);

	checkpointNext("NDC clip space (top, noclamp):");
	drawTR("  All inside (top, noclamp)", SIDE_TOP, 1.0f, 1.0f, 1.0f, 1.0f, false);
	drawTR("  X outside (top, noclamp)", SIDE_TOP, 2.0f, 1.0f, 1.0f, 1.0f, false);
	drawTR("  Y outside (top, noclamp)", SIDE_TOP, 1.0f, 2.0f, 1.0f, 1.0f, false);
	drawTR("  Z outside near (top, noclamp)", SIDE_TOP, 1.0f, 1.0f, 2.0f, 1.0f, false);
	drawTR("  Z outside far (top, noclamp)", SIDE_TOP, 1.0f, 1.0f, 1.0f, 2.0f, false);
	drawTR("  Z outside both (top, noclamp)", SIDE_TOP, 1.0f, 1.0f, 2.0f, 2.0f, false);

	checkpointNext("NDC clip space (right):");
	drawTR("  All inside (right)", SIDE_RIGHT, 1.0f, 1.0f, 1.0f, 1.0f);
	drawTR("  X outside (right)", SIDE_RIGHT, 2.0f, 1.0f, 1.0f, 1.0f);
	drawTR("  Y outside (right)", SIDE_RIGHT, 1.0f, 2.0f, 1.0f, 1.0f);
	drawTR("  Z outside near (right)", SIDE_RIGHT, 1.0f, 1.0f, 2.0f, 1.0f);
	drawTR("  Z outside far (right)", SIDE_RIGHT, 1.0f, 1.0f, 1.0f, 2.0f);
	drawTR("  Z outside both (right)", SIDE_RIGHT, 1.0f, 1.0f, 2.0f, 2.0f);

	checkpointNext("NDC clip space (right, noclamp):");
	drawTR("  All inside (right, noclamp)", SIDE_RIGHT, 1.0f, 1.0f, 1.0f, 1.0f, false);
	drawTR("  X outside (right, noclamp)", SIDE_RIGHT, 2.0f, 1.0f, 1.0f, 1.0f, false);
	drawTR("  Y outside (right, noclamp)", SIDE_RIGHT, 1.0f, 2.0f, 1.0f, 1.0f, false);
	drawTR("  Z outside near (right, noclamp)", SIDE_RIGHT, 1.0f, 1.0f, 2.0f, 1.0f, false);
	drawTR("  Z outside far (right, noclamp)", SIDE_RIGHT, 1.0f, 1.0f, 1.0f, 2.0f, false);
	drawTR("  Z outside both (right, noclamp)", SIDE_RIGHT, 1.0f, 1.0f, 2.0f, 2.0f, false);

	checkpointNext("Flat W clip:");
	drawWithW("  Flat W=1", 1.0f);
	drawWithW("  Flat W=2", 2.0f);
	drawWithW("  Flat W=0", 0.0f);
	drawWithW("  Flat W=0.001", 0.001f);
	drawWithW("  Flat W=-1", -1.0f);
	drawWithW("  Flat W=-2", -2.0f);

	checkpointNext("Flat W clip (noclamp):");
	drawWithW("  Flat W=1 (noclamp)", 1.0f, false);
	drawWithW("  Flat W=2 (noclamp)", 2.0f, false);
	drawWithW("  Flat W=0 (noclamp)", 0.0f, false);
	drawWithW("  Flat W=0.001 (noclamp)", 0.001f, false);
	drawWithW("  Flat W=-1 (noclamp)", -1.0f, false);
	drawWithW("  Flat W=-2 (noclamp)", -2.0f, false);

	checkpointNext("Linear W clip:");
	drawLinearW("  Linear W 1->1->2", 1.0f, 1.0f, 2.0f);
	drawLinearW("  Linear W 1->2->2", 1.0f, 2.0f, 2.0f);
	drawLinearW("  Linear W 1->1->epsilon", 1.0f, 1.0f, 0.0000001192092895507812500f);
	drawLinearW("  Linear W 1->epsilon->epsilon", 1.0f, 0.0000001192092895507812500f, 0.0000001192092895507812500f);
	drawLinearW("  Linear W 1->1->-0.001", 1.0f, 1.0f, -0.001f);
	drawLinearW("  Linear W 1->-0.001->-0.001", 1.0f, -0.001f, -0.001f);
	drawLinearW("  Linear W 1->1->-1", 1.0f, 1.0f, -1.0f);
	drawLinearW("  Linear W 1->-1->-1", 1.0f, -1.0f, -1.0f);
	drawLinearW("  Linear W -0.001->-0.001->-0.001", -0.001f, -0.001f, -0.001f);
	drawLinearW("  Linear W -1->-1->-1", -1.0f, -1.0f, -1.0f);

	delete[] copybuf;
	sceGuTerm();
	return 0;
}