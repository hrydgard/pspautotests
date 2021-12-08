#include "shared.h"

extern "C" int HAS_DISPLAY;

typedef struct {
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

inline VertexColorF32 makeVertex32(u32 c, float x, float y, float z) {
	VertexColorF32 v;
	v.color = c;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

void displayBuffer(const char *reason) {
	checkpoint("%s: COLOR=%08x", reason, readDispBuffer());
	//checkpoint("%s,%d", reason, readDispBuffer() & 0xFF);

	// Reset.
	clearDispBuffer(0x44444444);
}

void drawBoxCommands(u32 c) {
	vertices_f32[0] = makeVertex32(c, -1.0, -1.0, 0.0);
	vertices_f32[1] = makeVertex32(c, -0.5, 1.0, 0.0);

	// Clearing cache is fun.  Let's do it all the time.
	sceKernelDcacheWritebackInvalidateRange(vertices_f32, sizeof(vertices_f32));
	sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 2, NULL, vertices_f32);
}

void init() {
	initDisplay();
	clearDispBuffer(0x44444444);
	displayBuffer("Initial");
}

enum AlphaMode {
	ALPHA_DISABLE = -1,
	ALPHA_NORMAL = 0,
	ALPHA_TO_RGB = 1,
	ALPHA_DISABLE_TO_RGB = 2,
};

void testTexFunc(const char *title, u32 texc, u32 c, u32 texenv, bool doubling, AlphaMode alpha) {
	startFrame();

	if (alpha == ALPHA_TO_RGB || alpha == ALPHA_DISABLE_TO_RGB) {
		clearDispBuffer(0xFFFFFFFF);

		sceGuEnable(GU_BLEND);
		sceGuBlendFunc(GU_ADD, GU_FIX, GU_SRC_ALPHA, 0, 0);
		if (alpha == ALPHA_DISABLE_TO_RGB) {
			sceGuTexFunc(GU_TFX_BLEND, GU_TCC_RGB);
		} else {
			sceGuTexFunc(GU_TFX_BLEND, GU_TCC_RGBA);
		}
	} else {
		sceGuDisable(GU_BLEND);
		sceGuTexFunc(GU_TFX_BLEND, alpha == ALPHA_DISABLE ? GU_TCC_RGB : GU_TCC_RGBA);
	}

	sceGuTexEnvColor(texenv);

	if (doubling) {
		sceGuEnable(GU_FRAGMENT_2X);
	} else {
		sceGuDisable(GU_FRAGMENT_2X);
	}

	if (texdata[0] != texc || texdirty) {
		for (int i = 0; i < 16; ++i) {
			texdata[i] = texc;
		}
		sceKernelDcacheWritebackInvalidateRange(texdata, sizeof(texdata));

		sceGuTexImage(0, 1, 1, 1, texdata);
		sceGuTexFlush();
		sceGuTexSync();
		texdirty = false;
	}

	drawBoxCommands(c);

	endFrame();

	displayBuffer(title);

	sceGuDisable(GU_BLEND);
}

extern "C" int main(int argc, char *argv[]) {
	init();
	HAS_DISPLAY = 0;

	static const bool TEST_EXHAUSTIVE = false;

	checkpointNext("Common:");
	testTexFunc("  One + One", 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, false, ALPHA_NORMAL);
	testTexFunc("  One + Zero", 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, false, ALPHA_NORMAL);
	testTexFunc("  Zero + One", 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, false, ALPHA_NORMAL);
	testTexFunc("  Half + Half", 0x7F7F7F7F, 0x7F7F7F7F, 0xFFFFFFFF, false, ALPHA_NORMAL);
	testTexFunc("  7F + 3F", 0x7F7F7F7F, 0x3F3F3F3F, 0xFFFFFFFF, false, ALPHA_NORMAL);
	testTexFunc("  7F + 3F (RGB)", 0x7F7F7F7F, 0x3F3F3F3F, 0xFFFFFFFF, false, ALPHA_DISABLE);

	checkpointNext("Env colors:");
	testTexFunc("  One + Zero, half env", 0xFFFFFFFF, 0x00000000, 0x7F7F7F7F, false, ALPHA_NORMAL);
	testTexFunc("  Zero + One, half env", 0x00000000, 0xFFFFFFFF, 0x7F7F7F7F, false, ALPHA_NORMAL);

	checkpointNext("Doubling:");
	testTexFunc("  Half x2 + Half", 0x7F7F7F7F, 0x7F7F7F7F, 0x5F5F5F5F, true, ALPHA_NORMAL);
	testTexFunc("  One x2 + Half", 0xFFFFFFFF, 0x7F7F7F7F, 0x5F5F5F5F, true, ALPHA_NORMAL);
	testTexFunc("  Half x2 + One", 0x7F7F7F7F, 0xFFFFFFFF, 0x5F5F5F5F, true, ALPHA_NORMAL);
	testTexFunc("  Quarter x2 + Half", 0x3F3F3F3F, 0x7F7F7F7F, 0x5F5F5F5F, true, ALPHA_NORMAL);

	checkpointNext("Alpha:");
	testTexFunc("  Half + Half (alpha)", 0x7F000000, 0x7F000000, 0x5F5F5F5F, false, ALPHA_TO_RGB);
	testTexFunc("  Half x2 + Half (alpha)", 0x7F000000, 0x7F000000, 0x5F5F5F5F, true, ALPHA_TO_RGB);
	testTexFunc("  RGB func only", 0xFF7F3F7F, 0x3FFF7F7F, 0x5F5F5F5F, false, ALPHA_DISABLE_TO_RGB);
	testTexFunc("  RGB func only + double", 0xFF7F3F7F, 0x3FFF7F7F, 0x5F5F5F5F, true, ALPHA_DISABLE_TO_RGB);

	checkpointNext("Prim Rounding:");
	for (int i = 0; i < 256; ++i) {
		// (255 - texcolor) * prim + texcolor * texenv
		char temp[128];
		snprintf(temp, 128, "  Prim: 0x5F * 0x%02X + Zero (%f / %d)", i, i * 127.0 / 255.0, (i * 127) % 255);
		u32 primc = (0x5F << 24) | (0xFF << 16) | (i << 8) | i;
		testTexFunc(temp, 0x00808080, primc, 0x00000000, false, ALPHA_NORMAL);

		if (TEST_EXHAUSTIVE) {
			for (int j = 0; j < 256; ++j) {
				// Test each multiply by texcolor (gets inversed, though.)
				int jjjj = j | (j << 8) | (j << 16) | (j << 24);
				snprintf(temp, 128, "%d,%d", i, j);
				testTexFunc(temp, jjjj, primc, 0x00000000, false, ALPHA_NORMAL);
			}
			if (i != 255) {
				flushschedf();
			}
		}
	}

	checkpointNext("Env Rounding:");
	for (int i = 0; i < 256; ++i) {
		// (255 - texcolor) * prim + texcolor * texenv
		char temp[128];
		snprintf(temp, 128, "  Texenv: 0x7F * 0x%02X + Zero (%f / %d)", i, i * 127.0 / 255.0, (i * 127) % 255);
		u32 texc = (0x5F << 24) | (0xFF << 16) | (i << 8) | i;
		testTexFunc(temp, texc, 0x00000000, 0x7F7F7F7F, false, ALPHA_NORMAL);

		if (TEST_EXHAUSTIVE) {
			for (int j = 0; j < 256; ++j) {
				int jjjj = j | (j << 8) | (j << 16) | (j << 24);
				snprintf(temp, 128, "%d,%d", i, j);
				testTexFunc(temp, texc, 0x00000000, jjjj, false, ALPHA_NORMAL);
			}
			if (i != 255) {
				flushschedf();
			}
		}
	}

	checkpointNext("Alpha Rounding:");
	for (int i = 0; i < 256; ++i) {
		// (255 - texcolor) * prim + texcolor * texenv
		char temp[128];
		snprintf(temp, 128, "  Alpha: 0x7F * 0x%02X + Zero (%f / %d)", i, i * 127.0 / 255.0, (i * 127) % 255);
		u32 texc = (i << 24) | (i << 16) | (i << 8) | i;
		testTexFunc(temp, texc, 0x7F7F7F7F, 0x00000000, false, ALPHA_TO_RGB);

		if (TEST_EXHAUSTIVE) {
			for (int j = 0; j < 256; ++j) {
				int jjjj = j | (j << 8) | (j << 16) | (j << 24);
				snprintf(temp, 128, "%d,%d", i, j);
				testTexFunc(temp, texc, jjjj, 0x00000000, false, ALPHA_TO_RGB);
			}
			if (i != 255) {
				flushschedf();
			}
		}
	}

	sceGuTerm();

	return 0;
}
