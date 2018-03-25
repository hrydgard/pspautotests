#include <common.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include "../commands/commands.h"

extern "C" int sceDmacMemcpy(void *dest, const void *source, unsigned int size);

typedef struct {
	u32 color;
	float x, y, z;
} VertexColorF32;

enum TriangleType {
	TRIANGLE_ALL = -1,
	TRIANGLE_NORMAL = 0,
	TRIANGLE_AT_EDGE = 1,

	TRIANGLE_OUT_NEG_X = 2,
	TRIANGLE_OUT_NEG_Y = 3,
	TRIANGLE_OUT_NEG_Z = 4,

	TRIANGLE_OUT_POS_X = 5,
	TRIANGLE_OUT_POS_Y = 6,
	TRIANGLE_OUT_POS_Z = 7,
};

// +---------------------+  4096x4096 outer box
// |                     |  480x272 inner box
// |      +-------+      |
// |      |       |      |  Outer box at (-1808, -1912) - (2288, 2184)
// |      +-------+      |  Inner box at (0, 0) - (479, 271)
// |                     |
// +---------------------+
static __attribute__((aligned(16))) VertexColorF32 triangles[][3] = {
	// TRIANGLE_NORMAL
	{
		{0xFFFFFFFF, 0.0f, 0.0f, 0.0f},
		{0xFFFFFFFF, 0.0f, 272.0f, 0.5f},
		{0xFFFFFFFF, 480.0f, 272.0f, 1.0f},
	},
	// TRIANGLE_AT_EDGE
	{
		{0xFFFFFFFF, -1808.0f, -1912.0f, 0.0f},
		{0xFFFFFFFF, 0.0f, 2184.0f, 0.5f},
		{0xFFFFFFFF, 2288.0f, 2184.0f, 1.0f},
	},

	// TRIANGLE_OUT_NEG_X
	{
		{0xFFFFFFFF, -1809.0f, -1912.0f, 0.0f},
		{0xFFFFFFFF, 0.0f, 2184.0f, 0.5f},
		{0xFFFFFFFF, 2288.0f, 2184.0f, 1.0f},
	},
	// TRIANGLE_OUT_NEG_Y
	{
		{0xFFFFFFFF, -1808.0f, -1913.0f, 0.0f},
		{0xFFFFFFFF, 0.0f, 2184.0f, 0.5f},
		{0xFFFFFFFF, 2288.0f, 2184.0f, 1.0f},
	},
	// TRIANGLE_OUT_NEG_Z
	{
		{0xFFFFFFFF, -1808.0f, -1912.0f, -0.1f},
		{0xFFFFFFFF, 0.0f, 2184.0f, 0.5f},
		{0xFFFFFFFF, 2288.0f, 2184.0f, 1.0f},
	},

	// TRIANGLE_OUT_POS_X
	{
		{0xFFFFFFFF, 2289.0f, -1912.0f, 0.0f},
		{0xFFFFFFFF, -1808.0f, -1912.0f, 0.5f},
		{0xFFFFFFFF, 2289.0f, 2184.0f, 1.0f},
	},
	// TRIANGLE_OUT_POS_Y
	{
		{0xFFFFFFFF, 2288.0f, -1912.0f, 0.0f},
		{0xFFFFFFFF, -1808.0f, -1912.0f, 0.5f},
		{0xFFFFFFFF, 2288.0f, 2185.0f, 1.0f},
	},
	// TRIANGLE_OUT_POS_Z
	{
		{0xFFFFFFFF, -1808.0f, -1912.0f, 0.0f},
		{0xFFFFFFFF, 0.0f, 2184.0f, 0.5f},
		{0xFFFFFFFF, 2288.0f, 2184.0f, 1.1f},
	},
};

static u8 *fbp0 = 0;
static u8 *dbp0 = fbp0 + 512 * 272 * sizeof(u32);

static u32 *copybuf = NULL;
static unsigned int __attribute__((aligned(16))) list[32768];

inline VertexColorF32 makeVertex32(u32 c, float x, float y, float z) {
	VertexColorF32 v;
	v.color = c;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

void resetBuffer() {
	memset(copybuf, 0x44, 512 * 272 * 4);
	sceKernelDcacheWritebackInvalidateAll();
	sceDmacMemcpy(sceGeEdramGetAddr(), copybuf, 512 * 272 * 4);
	sceKernelDcacheWritebackInvalidateAll();
}

void displayBuffer(const char *reason) {
	sceKernelDcacheWritebackInvalidateAll();
	sceDmacMemcpy(copybuf, sceGeEdramGetAddr(), 512 * 272 * 4);
	sceKernelDcacheWritebackInvalidateAll();
	const u32 *buf = copybuf;

	bool found = false;
	for (int y = 0; y < 272 && !found; ++y) {
		for (int x = 0; x < 480; ++x) {
			if ((buf[y * 512 + x] & 0x00FFFFFF) == 0x00FFFFFF) {
				found = true;
				break;
			}
		}
	}
	checkpoint("%s: DRAW=%d", reason, found ? 1 : 0);
}

void drawBoxCommands(TriangleType tri) {
	// Clearing cache is fun.  Let's do it all the time.
	sceKernelDcacheWritebackInvalidateAll();
	if (tri >= 0) {
		sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 3, NULL, triangles[tri]);
	} else {
		sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, sizeof(triangles) / sizeof(triangles[0][0]), NULL, triangles[0]);
	}
	sceDisplaySetFrameBuf(sceGeEdramGetAddr(), 512, GU_PSM_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
}

void init() { 
	copybuf = new u32[512 * 272];

	sceGuInit();
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888, fbp0, 512);
	sceGuDispBuffer(480, 272, fbp0, 512);
	sceGuDepthBuffer(dbp0, 512);
	sceGuOffset(2048 - (480 / 2), 2048 - (272 / 2));
	sceGuViewport(2048, 2048, 480, 272);
	sceGuDepthRange(0, 65535);
	sceGuDepthMask(0);
	sceGuScissor(0, 0, 480, 272);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFrontFace(GU_CW);
	sceGuShadeModel(GU_SMOOTH);
	sceGuDisable(GU_TEXTURE_2D);
	sceGuDisable(GU_BLEND);

	sceGuSendCommandf(GE_CMD_VIEWPORTZ1, 32767.5);
	sceGuSendCommandf(GE_CMD_VIEWPORTZ2, 32767.5);
	sceGuSendCommandi(GE_CMD_MINZ, 0);
	sceGuSendCommandi(GE_CMD_MAXZ, 65535);

	ScePspFMatrix4 ones = {
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{0, 0, 0, 1},
	};

	ScePspFMatrix4 ortho = {
		{2.0f / 480.0f, 0, 0, 0},
		{0, -2.0f / 272.0f, 0, 0},
		{0, 0, 2.0f, 0},
		{-1.0f, 1.0f, -1.0f, 1.0f},
	};

	sceGuSetMatrix(GU_MODEL, &ones);
	sceGuSetMatrix(GU_VIEW, &ones);
	sceGuSetMatrix(GU_PROJECTION, &ortho);

	sceGuFinish();
	sceGuSync(0, 0);
 
	sceDisplayWaitVblankStart();
	sceGuDisplay(1);

	memset(copybuf, 0x44, 512 * 272 * 4);
	sceKernelDcacheWritebackInvalidateAll();
	sceDmacMemcpy(sceGeEdramGetAddr(), copybuf, 512 * 272 * 4);
	sceKernelDcacheWritebackInvalidateAll();

	displayBuffer("Initial");
}

void testTriangle(const char *title, TriangleType tri, bool clip = true) {
	resetBuffer();

	sceGuStart(GU_DIRECT, list);

	if (clip) {
		sceGuDisable(GU_CLIP_PLANES);
	} else {
		sceGuEnable(GU_CLIP_PLANES);
	}
	
	sceGuEnable(GU_DEPTH_TEST);
	sceGuDepthFunc(GU_ALWAYS);

	drawBoxCommands(tri);

	sceGuFinish();
	sceGuSync(GU_SYNC_WAIT, GU_SYNC_WHAT_DONE);
	sceDisplayWaitVblank();

	displayBuffer(title);
}

extern "C" int main(int argc, char *argv[]) {
	init();

	sceDisplaySetFrameBuf(sceGeEdramGetAddr(), 512, GU_PSM_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
	sceDisplaySetMode(0, 480, 272);

	testTriangle("  Normal, clipped", TRIANGLE_NORMAL, true);
	testTriangle("  Normal, unclipped", TRIANGLE_NORMAL, false);
	testTriangle("  Flat at edge, clipped", TRIANGLE_AT_EDGE, true);
	testTriangle("  Flat at edge, unclipped", TRIANGLE_AT_EDGE, false);

	checkpointNext("Flat out negative");
	testTriangle("  Flat out negative X, clipped", TRIANGLE_OUT_NEG_X, true);
	testTriangle("  Flat out negative X, unclipped", TRIANGLE_OUT_NEG_X, false);
	testTriangle("  Flat out negative Y, clipped", TRIANGLE_OUT_NEG_Y, true);
	testTriangle("  Flat out negative Y, unclipped", TRIANGLE_OUT_NEG_Y, false);
	testTriangle("  Flat out negative Z, clipped", TRIANGLE_OUT_NEG_Z, true);
	testTriangle("  Flat out negative Z, unclipped", TRIANGLE_OUT_NEG_Z, false);

	checkpointNext("Flat out positive");
	testTriangle("  Flat out positive X, clipped", TRIANGLE_OUT_POS_X, true);
	testTriangle("  Flat out positive X, unclipped", TRIANGLE_OUT_POS_X, false);
	testTriangle("  Flat out positive Y, clipped", TRIANGLE_OUT_POS_Y, true);
	testTriangle("  Flat out positive Y, unclipped", TRIANGLE_OUT_POS_Y, false);
	testTriangle("  Flat out positive Z, clipped", TRIANGLE_OUT_POS_Z, true);
	testTriangle("  Flat out positive Z, unclipped", TRIANGLE_OUT_POS_Z, false);

	checkpointNext("All (entire draw?)");
	testTriangle("  All, clipped", TRIANGLE_ALL, true);
	testTriangle("  All, unclipped", TRIANGLE_ALL, false);

	sceGuTerm();

	return 0;
}
