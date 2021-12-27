#include "shared.h"

extern "C" int HAS_DISPLAY;

enum FilterTestMode {
	TEST_CONST_BIAS = 0,
	TEST_AUTO_SIZE = 1,
	TEST_AUTO_SIZE_BIG = 2,
	TEST_SLOPE = 3,
};
static const FilterTestMode testMode = TEST_SLOPE;

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
static __attribute__((aligned(16))) u32 texdata2[16] = {
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

void drawBoxCommands(u32 c, float w_bias, float h_bias) {
	double wv = 2.0 / 240.0;
	double hv = 2.0 / 136.0;
	if (testMode == TEST_AUTO_SIZE_BIG) {
		wv = (64.0 - 32.0 * w_bias) / 240.0;
		hv = (64.0 - 32.0 * h_bias) / 136.0;
	} else if(testMode == TEST_AUTO_SIZE) {
		wv = (2.0 - w_bias) / 240.0;
		hv = (2.0 - h_bias) / 136.0;
	}

	vertices_f32[0] = makeVertex32(0.0f, 0.0f, c, -1.0f, 1.0f, 0.0);
	vertices_f32[1] = makeVertex32(1.0f, 1.0f, c, -1.0f + (float)wv, 1.0f - (float)hv, 0.0);

	// Clearing cache is fun.  Let's do it all the time.
	sceKernelDcacheWritebackInvalidateRange(vertices_f32, sizeof(vertices_f32));
	sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 2, NULL, vertices_f32);
}

void init() {
	initDisplay();
	clearDispBuffer(0x44444444);
	displayBuffer("Initial");
}

void testTexLinear(const char *title, u32 texc1, u32 texc2, float bias) {
	startFrame();

	sceGuTexFilter(GU_NEAREST_MIPMAP_LINEAR, GU_NEAREST_MIPMAP_LINEAR);
	sceGuTexMode(GU_PSM_8888, 1, 0, 0);
	if (testMode == TEST_AUTO_SIZE_BIG) {
		sceGuTexLevelMode(GU_TEXTURE_AUTO, 5.0f);
	} else if (testMode == TEST_AUTO_SIZE) {
		sceGuTexLevelMode(GU_TEXTURE_AUTO, 0.0f);
	} else if (testMode == TEST_CONST_BIAS) {
		sceGuTexLevelMode(GU_TEXTURE_CONST, bias);
	} else if (testMode == TEST_SLOPE) {
		sceGuTexLevelMode(GU_TEXTURE_SLOPE, 0.0f);
		sceGuTexSlope(bias);
	}

	if (texdata[0] != texc1 || texdata2[0] != texc2 || texdirty) {
		for (int i = 0; i < 16; ++i) {
			texdata[i] = texc1;
		}
		for (int i = 0; i < 16; ++i) {
			texdata2[i] = texc2;
		}
		sceKernelDcacheWritebackInvalidateRange(texdata, sizeof(texdata));
		sceKernelDcacheWritebackInvalidateRange(texdata2, sizeof(texdata2));

		sceGuTexImage(0, 2, 2, 4, texdata);
		sceGuTexImage(1, 1, 1, 4, texdata2);
		sceGuTexFlush();
		sceGuTexSync();
		texdirty = false;
	}

	drawBoxCommands(0xFFFFFFFF, bias, bias);

	endFrame();

	displayBuffer(title);
}

extern "C" int main(int argc, char *argv[]) {
	init();
	HAS_DISPLAY = 0;

	checkpointNext("Common:");
	testTexLinear("  Even", 0xFF0000FF, 0xFF00FF00, 0.0f);
	testTexLinear("  One level offset", 0xFF0000FF, 0xFF00FF00, 1.0f);

	checkpointNext("Offsets:");
	static const float offsets[] = { 1.0625f, 1.001953125f, 1.0009765625f, 1.00048828125f, 1.000244140625f, 1.0001220703125f, 0.5f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f, 0.0078125f, 0.00390625f, 0.001953125f };
	for (size_t i = 0; i < ARRAY_SIZE(offsets); ++i) {
		char temp[256];
		snprintf(temp, sizeof(temp), "  Offset %f", offsets[i]);
		testTexLinear(temp, 0xFF0000FF, 0xFF00FF00, offsets[i]);
	}
	for (int i = 0; i < 256; ++i) {
		char temp[256];
		float f = ((float)i / 256.0f) / 2.0f;
		u32 uf = *(u32 *)&f;
		snprintf(temp, sizeof(temp), "  Blend at %d - %02x / %08x", i, uf >> 23, uf & 0x07ffffff);
		testTexLinear(temp, 0xFFFDFEFF, 0x00000000, (float)i / 256.0f);
	}

	checkpointNext("Half blends:");
	for (int i = 0; i < 256; ++i) {
		u32 c = 0xFF000000 | (i << 16) | (i << 8) | i;
		char temp[256];
		snprintf(temp, sizeof(temp), "  %d * half", i);
		testTexLinear(temp, c, 0x00000000, 0.5f);
	}

	sceGuTerm();

	return 0;
}
