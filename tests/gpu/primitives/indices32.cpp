#include "shared.h"

// What the GE makes of 32-bit indices above 0xFFFF. Vertices 0-2 are a red triangle on the left,
// 0x10000-0x10002 a green one on the right, and everything else sits at the origin, so a triangle
// that picks any of those draws nothing.

static const int VERTEX_COUNT = 0x10003;
static Vertex_C8888_P16 __attribute__((aligned(16))) vertices[VERTEX_COUNT];

static u32 __attribute__((aligned(16))) indices32[3];
static u16 __attribute__((aligned(16))) indices16[3];

static void setTriangle(int first, u32 color, s16 x) {
	Vertex_C8888_P16 tri[3] = {
		{color, x, 10, 0},
		{color, (s16)(x + 40), 10, 0},
		{color, x, 50, 0},
	};
	for (int i = 0; i < 3; ++i) {
		vertices[first + i] = tri[i];
	}
}

static u32 readPixel(int x, int y) {
	const u32 *fb = (const u32 *)(0x44000000 | (u32)sceGeEdramGetAddr());
	return fb[y * BUF_WIDTH + x] & 0x00FFFFFF;
}

static const char *describe(u32 c) {
	if (c == 0) {
		return "nothing";
	} else if (c == 0x0000FF) {
		return "red";
	} else if (c == 0x00FF00) {
		return "green";
	}
	return "other";
}

static void drawWith(const char *title, int type, const void *indices) {
	startFrame();
	sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D | type, 3, indices, vertices);
	endFrame();
	checkpoint("  %s: left %s, right %s", title, describe(readPixel(20, 20)), describe(readPixel(120, 20)));
}

extern "C" int main(int argc, char *argv[]) {
	initDisplay();

	memset(vertices, 0, sizeof(vertices));
	setTriangle(0, 0xFF0000FF, 10);
	setTriangle(0x10000, 0xFF00FF00, 110);
	sceKernelDcacheWritebackInvalidateAll();

	checkpointNext("Indices:");
	indices16[0] = 0; indices16[1] = 1; indices16[2] = 2;
	sceKernelDcacheWritebackInvalidateAll();
	drawWith("16-bit 0, 1, 2", GU_INDEX_16BIT, indices16);

	indices32[0] = 0; indices32[1] = 1; indices32[2] = 2;
	sceKernelDcacheWritebackInvalidateAll();
	drawWith("32-bit 0, 1, 2", GU_INDEX_BITS, indices32);

	indices32[0] = 0x10000; indices32[1] = 0x10001; indices32[2] = 0x10002;
	sceKernelDcacheWritebackInvalidateAll();
	drawWith("32-bit 0x10000, 0x10001, 0x10002", GU_INDEX_BITS, indices32);

	indices32[0] = 0xFFFF0000; indices32[1] = 0xFFFF0001; indices32[2] = 0xFFFF0002;
	sceKernelDcacheWritebackInvalidateAll();
	drawWith("32-bit 0xFFFF0000, 0xFFFF0001, 0xFFFF0002", GU_INDEX_BITS, indices32);

	sceGuTerm();
	return 0;
}
