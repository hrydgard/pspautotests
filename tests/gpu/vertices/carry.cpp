#include "shared.h"

// What a draw whose vertex format has no texcoords (or no normal) uses instead.

// 16x16, in 4x4 blocks of one color each, so a through mode UV in the middle of a block is unambiguous.
static u32 __attribute__((aligned(16))) blockTex[16 * 16];

static const u16 __attribute__((aligned(16))) fanOrder[4] = { 1, 3, 0, 2 };

static const u32 THROUGH_UV = GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D;
static const u32 THROUGH = GU_VERTEX_16BIT | GU_TRANSFORM_2D;
static const u32 TRANSFORM_UV = GU_TEXTURE_32BITF | GU_VERTEX_16BIT | GU_TRANSFORM_3D;
static const u32 TRANSFORM = GU_VERTEX_16BIT | GU_TRANSFORM_3D;
static const u32 TRANSFORM_NRM = GU_NORMAL_32BITF | GU_VERTEX_16BIT | GU_TRANSFORM_3D;

static void initTexture() {
	for (int y = 0; y < 16; ++y) {
		for (int x = 0; x < 16; ++x) {
			blockTex[y * 16 + x] = COLORS[(y / 4) * 4 + (x / 4)];
		}
	}
	sceKernelDcacheWritebackRange(blockTex, sizeof(blockTex));
}

static void setTexture() {
	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
	sceGuTexFilter(GU_NEAREST, GU_NEAREST);
	sceGuTexFunc(GU_TFX_DECAL, GU_TCC_RGB);
	sceGuTexWrap(GU_CLAMP, GU_CLAMP);
	sceGuTexImage(0, 16, 16, 16, blockTex);
	sceGuTexScale(1.0f, 1.0f);
	sceGuTexOffset(0.0f, 0.0f);
	sceGuTexFlush();
}

static u32 readPixel(int x, int y) {
	const u32 *vram = (const u32 *)(0x44000000 | (u32)sceGeEdramGetAddr());
	return vram[y * 512 + x];
}

static void printBlock(const char *title) {
	const u32 c = readPixel(76, 26) & 0x00FFFFFF;
	for (int i = 0; i < 16; ++i) {
		if ((COLORS[i] & 0x00FFFFFF) == c) {
			printf("%s: block %d,%d\n", title, i % 4, i / 4);
			return;
		}
	}
	printf("%s: color %06x\n", title, (unsigned)c);
}

static void printColor(const char *title) {
	printf("%s: color %06x\n", title, (unsigned)(readPixel(76, 26) & 0x00FFFFFF));
}

// A quad at x, with the given UVs (in vertex order), drawn as a fan.
static void drawUV(u32 vtype, int x, const float uv[4][2], bool indexed = false) {
	Vertices vert(vtype);
	if (indexed) {
		// Positions so fanOrder makes a quad.
		vert.TP(uv[0][0], uv[0][1], x + 32, 42, 0);
		vert.TP(uv[1][0], uv[1][1], x, 10, 0);
		vert.TP(uv[2][0], uv[2][1], x, 42, 0);
		vert.TP(uv[3][0], uv[3][1], x + 32, 10, 0);
	} else {
		vert.TP(uv[0][0], uv[0][1], x, 10, 0);
		vert.TP(uv[1][0], uv[1][1], x + 32, 10, 0);
		vert.TP(uv[2][0], uv[2][1], x + 32, 42, 0);
		vert.TP(uv[3][0], uv[3][1], x, 42, 0);
	}
	void *p = sceGuGetMemory(vert.Size());
	memcpy(p, vert.Ptr(), vert.Size());
	if (indexed) {
		void *ind = sceGuGetMemory(sizeof(fanOrder));
		memcpy(ind, fanOrder, sizeof(fanOrder));
		sceGuDrawArray(GU_TRIANGLE_FAN, vtype | GU_INDEX_16BIT, 4, ind, p);
	} else {
		sceGuDrawArray(GU_TRIANGLE_FAN, vtype, 4, NULL, p);
	}
}

static void drawNormals(int x, const float n[4][3]) {
	Vertices vert(TRANSFORM_NRM);
	const int pos[4][2] = { { x, 10 }, { x + 32, 10 }, { x + 32, 42 }, { x, 42 } };
	for (int i = 0; i < 4; ++i) {
		vert.Normal(n[i][0], n[i][1], n[i][2]);
		vert.Pos(pos[i][0], pos[i][1], 0);
		vert.EndVert();
	}
	void *p = sceGuGetMemory(vert.Size());
	memcpy(p, vert.Ptr(), vert.Size());
	sceGuDrawArray(GU_TRIANGLE_FAN, TRANSFORM_NRM, 4, NULL, p);
}

// The quad that reads nothing, at x = 60.
static void drawBare(u32 vtype) {
	Vertices vert(vtype);
	vert.P(60, 10, 0);
	vert.P(92, 10, 0);
	vert.P(92, 42, 0);
	vert.P(60, 42, 0);
	void *p = sceGuGetMemory(vert.Size());
	memcpy(p, vert.Ptr(), vert.Size());
	sceGuDrawArray(GU_TRIANGLE_FAN, vtype, 4, NULL, p);
}

static void testThrough() {
	static const float same[4][2] = { { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 } };
	static const float varying[4][2] = { { 2, 2 }, { 6, 2 }, { 10, 10 }, { 14, 14 } };
	static const float order[4][2] = { { 2, 2 }, { 6, 6 }, { 10, 10 }, { 14, 14 } };

	startFrame();
	setTexture();
	drawUV(THROUGH_UV, 10, same);
	drawBare(THROUGH);
	endFrame();
	printBlock("Through, after 6,6");

	startFrame();
	setTexture();
	drawUV(THROUGH_UV, 10, varying);
	drawBare(THROUGH);
	endFrame();
	printBlock("Through, after varying UVs (last 14,14)");

	// Draw order 1, 3, 0, 2: the last read is vertex 2 (10,10), the highest vertex 3 (14,14).
	startFrame();
	setTexture();
	drawUV(THROUGH_UV, 10, order, true);
	drawBare(THROUGH);
	endFrame();
	printBlock("Through, indexed 1 3 0 2");

	// Across a list.
	startFrame();
	setTexture();
	drawUV(THROUGH_UV, 10, same);
	endFrame();
	startFrame();
	setTexture();
	drawBare(THROUGH);
	endFrame();
	printBlock("Through, next list");

	// A non-textured draw in between, with UVs.
	static const float other[4][2] = { { 14, 2 }, { 14, 2 }, { 14, 2 }, { 14, 2 } };
	startFrame();
	setTexture();
	drawUV(THROUGH_UV, 10, same);
	sceGuDisable(GU_TEXTURE_2D);
	drawUV(THROUGH_UV, 110, other);
	sceGuEnable(GU_TEXTURE_2D);
	drawBare(THROUGH);
	endFrame();
	printBlock("Through, untextured draw with 14,2 between");
}

static void testTransform() {
	// 7/16: the middle of block 1.
	static const float uv[4][2] = { { 0.4375f, 0.4375f }, { 0.4375f, 0.4375f }, { 0.4375f, 0.4375f }, { 0.4375f, 0.4375f } };

	startFrame();
	setTexture();
	drawUV(TRANSFORM_UV, 10, uv);
	drawBare(TRANSFORM);
	endFrame();
	printBlock("Transform, after 0.4375");

	// Halving the scale before the bare draw: block 0 if the raw UV is carried, block 1 if the scaled one.
	startFrame();
	setTexture();
	drawUV(TRANSFORM_UV, 10, uv);
	sceGuTexScale(0.5f, 0.5f);
	drawBare(TRANSFORM);
	endFrame();
	printBlock("Transform, scale 0.5 after");

	// And the other way: scaled while drawing the UVs, not after.
	startFrame();
	setTexture();
	sceGuTexScale(0.5f, 0.5f);
	drawUV(TRANSFORM_UV, 10, uv);
	sceGuTexScale(1.0f, 1.0f);
	drawBare(TRANSFORM);
	endFrame();
	printBlock("Transform, scale 0.5 before");

	// Through mode UVs, then a transformed draw.
	static const float through[4][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
	startFrame();
	setTexture();
	drawUV(TRANSFORM_UV, 10, uv);
	drawUV(THROUGH_UV, 110, through);
	drawBare(TRANSFORM);
	endFrame();
	printBlock("Transform, after through 0,0");
}

static void setLight() {
	sceGuDisable(GU_TEXTURE_2D);
	sceGuEnable(GU_LIGHTING);
	sceGuEnable(GU_LIGHT0);
	ScePspFVector3 dir = { 0.0f, 0.0f, 1.0f };
	sceGuLight(0, GU_DIRECTIONAL, GU_DIFFUSE, &dir);
	sceGuLightColor(0, GU_DIFFUSE, 0xFFFFFFFF);
	sceGuLightAtt(0, 1.0f, 0.0f, 0.0f);
	sceGuAmbient(0xFF000000);
	sceGuColor(0xFFFFFFFF);
}

static void testNormals() {
	static const float tilted[4][3] = { { 0, 0.6f, 0.8f }, { 0, 0.6f, 0.8f }, { 0, 0.6f, 0.8f }, { 0, 0.6f, 0.8f } };
	static const float varying[4][3] = { { 0, 0.6f, 0.8f }, { 0, 0.8f, 0.6f }, { 0, 0.6f, 0.8f }, { 0, 0, 1 } };

	startFrame();
	setLight();
	drawNormals(10, tilted);
	drawBare(TRANSFORM);
	endFrame();
	printColor("Normal, after 0 0.6 0.8");

	startFrame();
	setLight();
	drawNormals(10, varying);
	drawBare(TRANSFORM);
	endFrame();
	printColor("Normal, after varying (last 0 0 1)");

	startFrame();
	setLight();
	drawNormals(10, tilted);
	sceGuDisable(GU_LIGHTING);
	drawBare(TRANSFORM);
	sceGuEnable(GU_LIGHTING);
	drawBare(TRANSFORM);
	endFrame();
	printColor("Normal, unlit bare draw between");

	sceGuStart(GU_DIRECT, list);
	sceGuDisable(GU_LIGHTING);
	sceGuDisable(GU_LIGHT0);
	sceGuFinish();
	sceGuSync(0, 0);
}

extern "C" int main(int argc, char *argv[]) {
	initDisplay();
	initTexture();

	testThrough();
	testTransform();
	testNormals();

	sceGuTerm();
	return 0;
}
