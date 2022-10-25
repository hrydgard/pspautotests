#include "shared.h"

static const int BOX_W = 32;
static const int BOX_H = 32;
static const int BOX_OFF = 4;
static const float BOX_UV_OFF = 0.5f;
static const int BOX_SPACE = 10;

static u32 __attribute__((aligned(16))) imageData[4][4] = {
	{COLORS[0], COLORS[1], COLORS[2], COLORS[3]},
	{COLORS[1], COLORS[2], COLORS[3], COLORS[4]},
	{COLORS[2], COLORS[3], COLORS[4], COLORS[5]},
	{COLORS[3], COLORS[4], COLORS[5], COLORS[6]},
};

void writeOffCP(Vertices &vert, int morphCount, int morphType, float u, float v, int x, int y) {
	vert.Texcoord(u, v);
	vert.Color(COLORS[0]);
	vert.Normal(5, 5, 1);
	vert.Pos(x, y, 0);
	for (int i = 1; i < morphCount; ++i) {
		if (morphType == 0) {
			vert.Texcoord(u + BOX_UV_OFF, v + BOX_UV_OFF);
			vert.Color(COLORS[i]);
			vert.Normal(1, 1, 1);
			vert.Pos(x + BOX_OFF, y + BOX_OFF, 0);
		} else {
			vert.Texcoord(BOX_UV_OFF, BOX_UV_OFF);
			vert.Color(COLORS[i]);
			vert.Normal(0, 0, 0);
			vert.PosDirect(0, 0, 0);
		}
	}
	vert.EndVert();
}

void nextBox(int t, int morphType = 0, bool useInds = false) {
	static int x = 10;
	static int y = 10;

	int morphCount = ((t >> 18) & 7) + 1;

	Vertices vert(t);
	if (useInds) {
		writeOffCP(vert, morphCount, morphType, 2.0f, 0.0f, x - 5, y + 5);
	}
	writeOffCP(vert, morphCount, morphType, 0.0f, 0.0f, x, y);
	writeOffCP(vert, morphCount, morphType, 1.0f, 1.0f, x + BOX_W, y + BOX_H);
	vert.EndVert();

	void *p = sceGuGetMemory(vert.Size() + (useInds ? 4 : 0));
	memcpy(p, vert.Ptr(), vert.Size());
	u16 *inds = NULL;
	if (useInds) {
		inds = (u16 *)((u8 *)p + vert.Size());
		inds[0] = 1;
		inds[1] = 2;
	}
	sceGuDrawArray(GU_SPRITES, vert.VType(), 2, inds, p);

	x += BOX_W + BOX_SPACE;
	if (x + BOX_W >= 480) {
		x = 10;
		y += BOX_H + BOX_SPACE;
	}
}

void draw() {
	startFrame();

	sceGuMorphWeight(0, 0.5f);
	sceGuMorphWeight(1, 0.5f);
	nextBox(GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D | GU_VERTICES(1));
	nextBox(GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D | GU_VERTICES(2));

	sceGuMorphWeight(0, 0.75f);
	sceGuMorphWeight(1, 0.25f);
	nextBox(GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D | GU_VERTICES(2));
	
	sceGuMorphWeight(0, 0.25f);
	sceGuMorphWeight(1, 0.25f);
	sceGuMorphWeight(2, 0.25f);
	sceGuMorphWeight(3, 0.25f);
	nextBox(GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D | GU_VERTICES(4));
	
	sceGuMorphWeight(0, 1.0f);
	sceGuMorphWeight(1, 0.25f);
	sceGuMorphWeight(2, 0.25f);
	sceGuMorphWeight(3, 0.25f);
	nextBox(GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D | GU_VERTICES(4), 1);
	
	sceGuMorphWeight(0, 1.0f);
	sceGuMorphWeight(1, 1.0f);
	sceGuMorphWeight(2, 1.0f);
	sceGuMorphWeight(3, 1.0f);
	nextBox(GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D | GU_VERTICES(4), 1);

	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
	sceGuTexFilter(GU_NEAREST, GU_NEAREST);
	sceGuTexFunc(GU_TFX_DECAL, GU_TCC_RGB);
	sceGuTexWrap(GU_REPEAT, GU_REPEAT);
	sceGuTexImage(0, 4, 4, 4, imageData);

	nextBox(GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D | GU_VERTICES(1));
	nextBox(GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D | GU_VERTICES(2), 1);

	sceGuDisable(GU_TEXTURE_2D);
	sceGuEnable(GU_LIGHTING);
	sceGuEnable(GU_LIGHT0);
	ScePspFVector3 lightpos = {1, 1, 1};
	ScePspFVector3 lightdir = {1, 1, 1};
	sceGuLight(0, GU_DIRECTIONAL, GU_AMBIENT_AND_DIFFUSE, &lightpos);
	sceGuLightMode(GU_SINGLE_COLOR);
	sceGuLightSpot(0, &lightdir, 1.0f, 1.0f);
	sceGuLightColor(0, GU_AMBIENT_AND_DIFFUSE, 0xFFFF7F);

	nextBox(GU_COLOR_8888 | GU_NORMAL_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D | GU_VERTICES(1));

	// Weird things happen with GU_TRANSFORM_2D, and also crashes.

	// Let's try clear mode.
	sceGuSendCommandi(211, 1 | ((GU_COLOR_BUFFER_BIT | GU_STENCIL_BUFFER_BIT) << 8));

	sceGuMorphWeight(0, 0.5f);
	sceGuMorphWeight(1, 0.5f);
	nextBox(GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D | GU_VERTICES(1));
	nextBox(GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D | GU_VERTICES(2));

	sceGuSendCommandi(211, 0);

	nextBox(GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D | GU_VERTICES(2) | GU_INDEX_16BIT, 0, true);

	endFrame();
}

extern "C" int main(int argc, char *argv[]) {
	initDisplay();

	draw();

	emulatorEmitScreenshot();
	sceGuTerm();

	return 0;
}
