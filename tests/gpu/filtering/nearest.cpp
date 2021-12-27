#include "shared.h"

extern "C" int HAS_DISPLAY;

typedef struct {
	float u, v;
	u32 color;
	float x, y, z;
} VertexColorF32;

static __attribute__((aligned(16))) VertexColorF32 vertices_f32[256];
static __attribute__((aligned(16))) u32 texdata[16] = {
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};
static bool texdirty = true;

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
	checkpoint("%s: COLOR=%08x", reason, readDispBuffer());
	//checkpoint("%s,%d", reason, readDispBuffer() & 0xFF);

	// Reset.
	clearDispBuffer(0x44444444);
}

void drawBoxCommands(u32 c, float offsetX, float offsetY) {
	vertices_f32[0] = makeVertex32(0.0f + offsetX, 0.0f + offsetY, c, -1.0f, 1.0f, 0.0);
	vertices_f32[1] = makeVertex32(1.0f + offsetX, 1.0f + offsetY, c, -1.0f + 2 / 240.0f, 1.0f - 2 / 136.0f, 0.0);

	// Clearing cache is fun.  Let's do it all the time.
	sceKernelDcacheWritebackInvalidateRange(vertices_f32, sizeof(vertices_f32));
	sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 2, NULL, vertices_f32);
}

void init() {
	initDisplay();
	clearDispBuffer(0x44444444);
	displayBuffer("Initial");
}

void testTexLinear(const char *title, u32 texc1, u32 texc2, float offsetX = 0.0f, float offsetY = 0.0f) {
	startFrame();

	sceGuTexFilter(GU_NEAREST, GU_NEAREST);

	if (texdata[0] != texc1 || texdata[1] != texc2 || texdirty) {
		for (int i = 0; i < 16; ++i) {
			texdata[i] = (i & 1) != 0 ? texc2 : texc1;
		}
		sceKernelDcacheWritebackInvalidateRange(texdata, sizeof(texdata));

		sceGuTexImage(0, 2, 2, 4, texdata);
		sceGuTexFlush();
		sceGuTexSync();
		texdirty = false;
	}

	drawBoxCommands(0xFFFFFFFF, offsetX, offsetY);

	endFrame();

	displayBuffer(title);
}

void testTexLinearXY(const char *title, u32 texc1, u32 texc2, float offsetX = 0.0f, float offsetY = 0.0f) {
	startFrame();

	sceGuTexFilter(GU_NEAREST, GU_NEAREST);

	if (texdata[0] != texc1 || texdata[1] != texc2 || texdata[4] != texc2 || texdata[5] != texc2 || texdirty) {
		for (int i = 0; i < 16; ++i) {
			texdata[i] = i != 0 ? texc2 : texc1;
		}
		sceKernelDcacheWritebackInvalidateRange(texdata, sizeof(texdata));

		sceGuTexImage(0, 2, 2, 4, texdata);
		sceGuTexFlush();
		sceGuTexSync();
		texdirty = false;
	}

	drawBoxCommands(0xFFFFFFFF, offsetX, offsetY);

	endFrame();

	displayBuffer(title);
}

extern "C" int main(int argc, char *argv[]) {
	init();
	HAS_DISPLAY = 0;

	checkpointNext("Common:");
	testTexLinear("  Even", 0xFF0000FF, 0xFF00FF00, 0.0f);
	testTexLinear("  One texel offset", 0xFF0000FF, 0xFF00FF00, 0.5f);
	testTexLinearXY("  X and Y", 0xFF0000FF, 0xFF00FF00, 0.5f, 0.5f);
	testTexLinearXY("  Half X and Y", 0xFF0000FF, 0xFF00FF00, 0.25f, 0.25f);

	checkpointNext("Offsets:");
	static const float offsets[] = { 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f, 0.0078125f, 0.00390625f, 0.001953125f };
	for (size_t i = 0; i < ARRAY_SIZE(offsets); ++i) {
		char temp[256];
		snprintf(temp, sizeof(temp), "  Offset %f", offsets[i]);
		testTexLinear(temp, 0xFF0000FF, 0xFF00FF00, offsets[i]);
	}
	for (int i = 0; i < 256; ++i) {
		char temp[256];
		snprintf(temp, sizeof(temp), "  Blend at %d", i);
		testTexLinear(temp, 0xFFFDFEFF, 0x00000000, (float)i / 512.0f);
	}

	checkpointNext("Half blends:");
	for (int i = 0; i < 256; ++i) {
		u32 c = 0xFF000000 | (i << 16) | (i << 8) | i;
		char temp[256];
		snprintf(temp, sizeof(temp), "  %d * half", i);
		testTexLinear(temp, c, 0x00000000, 0.25f);
	}

	checkpointNext("Pos offsets:");
	for (int i = 0; i < 16; ++i) {
		sceGuStart(GU_DIRECT, list);
		sceGuSendCommandi(76, ((2048 - (480 / 2)) << 4) + i);
		sceGuSendCommandi(77, ((2048 - (272 / 2)) << 4) + i);
		sceGuFinish();
		sceGuSync(GU_SYNC_WAIT, GU_SYNC_WHAT_DONE);

		char temp[256];
		snprintf(temp, sizeof(temp), "  Pos offset %d", i);
		testTexLinear(temp, 0xFF0000FF, 0xFF00FF00, 0.0f);
	}

	sceGuTerm();

	return 0;
}
