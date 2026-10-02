#include <common.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include "../commands/commands.h"

extern "C" int sceDmacMemcpy(void *dest, const void *source, unsigned int size);

// A primitive whose vertices are all outside the same x or y clip plane (|x| > w) is culled, like z, even
// though there are no x/y clip planes and it would land on screen. The viewport scale is 10 here, so clip
// x of 1.25 - 2.5 is still well inside the screen.

typedef struct {
	u32 color;
	float x, y, z;
} VertexColorF32;

static u8 *fbp0 = 0;
static u8 *dbp0 = fbp0 + 512 * 272 * sizeof(u32);

static u32 *copybuf = NULL;
static unsigned int __attribute__((aligned(16))) list[32768];
static VertexColorF32 __attribute__((aligned(16))) verts[8];

static const float STEP = 1.0f / 32768.0f;

void resetBuffer() {
	memset(copybuf, 0, 512 * 272 * 4);
	sceKernelDcacheWritebackInvalidateAll();
	sceDmacMemcpy(sceGeEdramGetAddr(), copybuf, 512 * 272 * 4);
	sceKernelDcacheWritebackInvalidateAll();
}

int countDrawn() {
	sceKernelDcacheWritebackInvalidateAll();
	sceDmacMemcpy(copybuf, sceGeEdramGetAddr(), 512 * 272 * 4);
	sceKernelDcacheWritebackInvalidateAll();
	int count = 0;
	for (int y = 0; y < 272; ++y) {
		for (int x = 0; x < 480; ++x) {
			if ((copybuf[y * 512 + x] & 0x00FFFFFF) != 0)
				count++;
		}
	}
	return count;
}

// a is along the axis under test (times sign), b along the other one. Positions are scaled by w, so the
// projection's w leaves x / w as given.
static void setVert(int i, char axis, int sign, float a, float b, float w) {
	verts[i].color = 0xFFFFFFFF;
	verts[i].x = (axis == 'x' ? sign * a : b) * w;
	verts[i].y = (axis == 'x' ? b : sign * a) * w;
	verts[i].z = 0.0f;
}

static void draw(int prim, int count, bool clip, float w) {
	resetBuffer();
	sceKernelDcacheWritebackInvalidateRange(verts, sizeof(verts));

	sceGuStart(GU_DIRECT, list);
	if (clip) {
		sceGuEnable(GU_CLIP_PLANES);
	} else {
		sceGuDisable(GU_CLIP_PLANES);
	}
	ScePspFMatrix4 proj = {
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{0, 0, 0, w},
	};
	sceGuSetMatrix(GU_PROJECTION, &proj);
	sceGuDrawArray(prim, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, count, NULL, verts);
	sceGuFinish();
	sceGuSync(GU_SYNC_WAIT, GU_SYNC_WHAT_DONE);
}

static void testShapes(const char *title, bool clip, float w) {
	checkpointNext(title);
	static const char axes[2] = { 'x', 'y' };
	for (int ai = 0; ai < 2; ++ai) {
		for (int sign = 1; sign >= -1; sign -= 2) {
			char axis = axes[ai];
			char tag[3] = { sign > 0 ? '+' : '-', axis, 0 };

			setVert(0, axis, sign, 1.25f, -0.5f, w);
			setVert(1, axis, sign, 2.5f, 0.0f, w);
			setVert(2, axis, sign, 1.5f, 0.75f, w);
			draw(GU_TRIANGLES, 3, clip, w);
			checkpoint("  Triangle all beyond %s: %d", tag, countDrawn() != 0);

			setVert(0, axis, sign, 1.0f, -0.5f, w);
			draw(GU_TRIANGLES, 3, clip, w);
			checkpoint("  Triangle one on the plane %s: %d", tag, countDrawn() != 0);

			setVert(0, axis, sign, 1.0f + STEP, -0.5f, w);
			draw(GU_TRIANGLES, 3, clip, w);
			checkpoint("  Triangle one a step past %s: %d", tag, countDrawn() != 0);

			setVert(0, axis, sign, 0.5f, -0.5f, w);
			draw(GU_TRIANGLES, 3, clip, w);
			checkpoint("  Triangle one inside %s: %d", tag, countDrawn() != 0);

			setVert(0, axis, sign, 1.25f, -0.5f, w);
			setVert(1, axis, sign, 2.5f, 0.5f, w);
			draw(GU_LINES, 2, clip, w);
			checkpoint("  Line all beyond %s: %d", tag, countDrawn() != 0);

			setVert(0, axis, sign, 1.0f, -0.5f, w);
			draw(GU_LINES, 2, clip, w);
			checkpoint("  Line one on the plane %s: %d", tag, countDrawn() != 0);

			setVert(0, axis, sign, 1.5f, -0.5f, w);
			draw(GU_POINTS, 1, clip, w);
			checkpoint("  Point beyond %s: %d", tag, countDrawn() != 0);

			setVert(0, axis, sign, 1.0f, -0.5f, w);
			draw(GU_POINTS, 1, clip, w);
			checkpoint("  Point on the plane %s: %d", tag, countDrawn() != 0);

			setVert(0, axis, sign, 1.25f, -0.5f, w);
			setVert(1, axis, sign, 2.5f, 0.5f, w);
			draw(GU_SPRITES, 2, clip, w);
			checkpoint("  Rectangle all beyond %s: %d", tag, countDrawn() != 0);

			setVert(0, axis, sign, 0.5f, -0.5f, w);
			draw(GU_SPRITES, 2, clip, w);
			checkpoint("  Rectangle one inside %s: %d", tag, countDrawn() != 0);
		}
	}

	// Outside on different sides, none inside: not culled.
	setVert(0, 'x', 1, 1.5f, -1.5f, w);
	setVert(1, 'x', -1, 1.5f, 1.5f, w);
	setVert(2, 'x', 1, 1.75f, 1.75f, w);
	draw(GU_TRIANGLES, 3, clip, w);
	checkpoint("  Triangle beyond x and y corners: %d", countDrawn() != 0);

	setVert(0, 'x', -1, 1.5f, -0.5f, w);
	setVert(1, 'x', 1, 1.5f, -0.5f, w);
	setVert(2, 'x', 1, 0.0f, -2.0f, w);
	draw(GU_TRIANGLES, 3, clip, w);
	checkpoint("  Triangle across x: %d", countDrawn() != 0);
}

void init() {
	copybuf = new u32[512 * 272];

	sceGuInit();
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888, fbp0, 512);
	sceGuDispBuffer(480, 272, fbp0, 512);
	sceGuDepthBuffer(dbp0, 512);
	sceGuOffset(2048 - (480 / 2), 2048 - (272 / 2));
	sceGuSendCommandf(GE_CMD_VIEWPORTX1, 10.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTY1, 10.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTX2, 2048.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTY2, 2048.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTZ1, 32767.5f);
	sceGuSendCommandf(GE_CMD_VIEWPORTZ2, 32767.5f);
	sceGuSendCommandi(GE_CMD_MINZ, 0);
	sceGuSendCommandi(GE_CMD_MAXZ, 65535);
	sceGuScissor(0, 0, 480, 272);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuDisable(GU_DEPTH_TEST);
	sceGuDisable(GU_CULL_FACE);
	sceGuShadeModel(GU_FLAT);
	sceGuDisable(GU_TEXTURE_2D);
	sceGuDisable(GU_BLEND);

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
}

extern "C" int main(int argc, char *argv[]) {
	init();

	sceDisplaySetFrameBuf(sceGeEdramGetAddr(), 512, GU_PSM_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
	sceDisplaySetMode(0, 480, 272);

	testShapes("Clipping on", true, 1.0f);
	testShapes("Clipping off", false, 1.0f);
	testShapes("Clipping on, w = 2", true, 2.0f);

	sceGuTerm();

	return 0;
}
