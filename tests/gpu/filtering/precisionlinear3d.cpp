#include "shared.h"

extern "C" int HAS_DISPLAY;

typedef struct {
	float u, v;
	u32 color;
	float x, y, z;
} VertexColorF32;

static __attribute__((aligned(16))) VertexColorF32 vertices_f32[256];
static __attribute__((aligned(16))) u32 texdata[16] = {
	0xFFFFFFFF, 0xFF000000, 0xFFFFFFFF, 0xFFFFFFFF,
	0xFF0000FF, 0xFF00FF00, 0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};

inline VertexColorF32 makeVertex32(float u, float v, u32 c, float x, float y, float z) {
	VertexColorF32 vv;
	vv.u = u;
	vv.v = v;
	vv.color = c;
	vv.x = x;
	vv.y = y;
	vv.z = z;
	return vv;
}

void displayBuffer(const char *reason) {
	u32 tl = readFullDispBuffer(0, 0);

	u32 red = tl;
	int redpos = -1;
	for (int y = 1; y < 272; ++y) {
		u32 c = readFullDispBuffer(0, y);
		if (c != tl) {
			red = c;
			redpos = y;
			break;
		}
	}

	u32 black = tl;
	int blackpos = -1;
	for (int x = 1; x < 480; ++x) {
		u32 c = readFullDispBuffer(x, 0);
		if (c != tl) {
			black = c;
			blackpos = x;
			break;
		}
	}

	checkpoint("%s: COLOR=%08x, left=%d, %08x, top=%d, %08x", reason, tl, redpos, red, blackpos, black);

	// Reset.
	clearDispBuffer(0x44444444);
}

void init() {
	initDisplay();

	setNeedFull(true);
	clearDispBuffer(0x44444444);
	displayBuffer("Initial");

	startFrame();
	sceGuTexFilter(GU_LINEAR, GU_LINEAR);
	sceGuTexImage(0, 2, 2, 4, texdata);
	sceGuTexWrap(GU_CLAMP, GU_CLAMP);
	// Let's simply scale the UVs and pos to match through.
	sceGuTexScale(0.5f, 0.5f);
	sceGuOffset(2048, 2048);
	sceGuViewport(2048, 2048, 2, -2);
	sceGuSendCommandf(68, 1.0f);
	sceGuSendCommandf(71, 0.0f);
	sceGuTexFlush();
	sceGuTexSync();
	endFrame();
}

void testTexMagnify(const char *title, int w, int h, float xoff, float yoff) {
	startFrame();
	if (w >= 0 && h >= 0) {
		vertices_f32[0] = makeVertex32(0.0f + xoff, 0.0f + yoff, 0xFFFFFFFF, 0.0f, 0.0f, 0.0f);
		vertices_f32[1] = makeVertex32(2.0f + xoff, 2.0f + yoff, 0xFFFFFFFF, w, h, 0.0f);
	} else if (w >= 0) {
		vertices_f32[0] = makeVertex32(0.0f + xoff, 0.0f + yoff, 0xFFFFFFFF, 0.0f, -h, 0.0f);
		vertices_f32[1] = makeVertex32(2.0f + xoff, 2.0f + yoff, 0xFFFFFFFF, w, 0.0f, 0.0f);
	} else if (h >= 0) {
		vertices_f32[0] = makeVertex32(0.0f + xoff, 0.0f + yoff, 0xFFFFFFFF, -w, 0.0f, 0.0f);
		vertices_f32[1] = makeVertex32(2.0f + xoff, 2.0f + yoff, 0xFFFFFFFF, 0.0f, h, 0.0f);
	} else {
		vertices_f32[0] = makeVertex32(0.0f + xoff, 0.0f + yoff, 0xFFFFFFFF, -w, -h, 0.0f);
		vertices_f32[1] = makeVertex32(2.0f + xoff, 2.0f + yoff, 0xFFFFFFFF, 0.0f, 0.0f, 0.0f);
	}

	// Clearing cache is fun.  Let's do it all the time.
	sceKernelDcacheWritebackInvalidateRange(vertices_f32, sizeof(vertices_f32));
	sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 2, NULL, vertices_f32);
	dirtyDispBuffer();
	endFrame();

	displayBuffer(title);
}

extern "C" int main(int argc, char *argv[]) {
	init();
	HAS_DISPLAY = 0;

	checkpointNext("Orientations:");
	testTexMagnify("  TL -> BR", 256.0f, 256.0f, 0.0f, 0.0f);
	testTexMagnify("  BR -> TL", -256.0f, -256.0f, 0.0f, 0.0f);
	testTexMagnify("  TR -> BL", -256.0f, 256.0f, 0.0f, 0.0f);
	testTexMagnify("  BL -> TR", 256.0f, -256.0f, 0.0f, 0.0f);
	testTexMagnify("  TL -> BR (small)", 2.0f, 2.0f, 0.0f, 0.0f);

	checkpointNext("Offsets");
	testTexMagnify("  One texel", 256.0f, 256.0f, 1.0f, 1.0f);
	testTexMagnify("  Half texel", 256.0f, 256.0f, 0.5f, 0.5f);
	testTexMagnify("  Negative half texel", 256.0f, 256.0f, -0.5f, -0.5f);
	testTexMagnify("  Half texel (small)", 2.0f, 2.0f, 0.5f, 0.5f);

	checkpointNext("Pos X offsets:");
	for (int i = 0; i < 16; ++i) {
		sceGuStart(GU_DIRECT, list);
		sceGuSendCommandi(76, 2048 * 16 + i);
		sceGuSendCommandi(77, 2048 * 16);
		sceGuFinish();
		sceGuSync(GU_SYNC_WAIT, GU_SYNC_WHAT_DONE);

		char temp[256];
		snprintf(temp, sizeof(temp), "  Pos X offset %d", i);
		testTexMagnify(temp, 2.0f, 2.0f, 0.0f, 0.0f);
		snprintf(temp, sizeof(temp), "  Pos X offset (256x) %d", i);
		testTexMagnify(temp, 512.0f, 512.0f, 0.0f, 0.0f);
	}

	checkpointNext("Pos Y offsets:");
	for (int i = 0; i < 16; ++i) {
		sceGuStart(GU_DIRECT, list);
		sceGuSendCommandi(76, 2048 * 16);
		sceGuSendCommandi(77, 2048 * 16 + i);
		sceGuFinish();
		sceGuSync(GU_SYNC_WAIT, GU_SYNC_WHAT_DONE);

		char temp[256];
		snprintf(temp, sizeof(temp), "  Pos Y offset %d", i);
		testTexMagnify(temp, 2.0f, 2.0f, 0.0f, 0.0f);
		snprintf(temp, sizeof(temp), "  Pos Y offset (256x) %d", i);
		testTexMagnify(temp, 512.0f, 512.0f, 0.0f, 0.0f);
	}

	sceGuTerm();

	return 0;
}
