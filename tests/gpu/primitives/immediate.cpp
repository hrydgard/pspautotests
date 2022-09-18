#include "shared.h"
#include "../commands/commands.h"

extern unsigned int HAS_DISPLAY;

#define GU_CONTINUE 7

// Matters only on the first vert.
#define GU_IMM_SHADE_MODE    0x00040000
#define GU_IMM_CULL          0x00080000
#define GU_IMM_CULL_MODE     0x00100000
#define GU_IMM_TEXTURE       0x00200000
#define GU_IMM_FOG           0x00400000
#define GU_IMM_DITHER        0x00800000

// TODO: Not tested.
#define GU_IMM_ANTIALIAS     0x00000800
// TODO: Not sure what this does...
#define GU_IMM_CLIP_MASK     0x0003F000

static u32 __attribute__((aligned(16))) imageData[4][4] = {
	{0xFF000000, 0xFF111111, 0xFF222222, 0xFF333333},
	{0xFF444444, 0xFF555555, 0xFF666666, 0xFF777777},
	{0xFF888888, 0xFF999999, 0xFFAAAAAA, 0xFFBBBBBB},
	{0xFFCCCCCC, 0xFFDDDDDD, 0xFFEEEEEE, 0xFFFFFFFF},
};

Vertex_C8888_P16 vertices1[4] = {
	{0xFF0000FF, 10, 10, 0x1234},
	{0xFF0000FF, 50, 10, 0x1234},
	{0xFF0000FF, 50, 50, 0x1234},
	{0xFF0000FF, 10, 50, 0x1234},
};

Vertex_C8888_P16 vertices2[4] = {
	{0xFF00FF00, 60, 10, 0},
	{0xFF00FF00, 100, 10, 0},
	{0xFF00FF00, 100, 50, 0},
	{0xFF00FF00, 60, 50, 0},
};

Vertex_C8888_P16 vertices3[4] = {
	{0xFFFF0000, 110, 10, 0},
	{0xFFFF0000, 150, 10, 0},
	{0xFFFF0000, 150, 50, 0},
	{0xFFFF0000, 110, 50, 0},
};

Vertex_C8888_P16 vertices4[4] = {
	{0xFF00FFFF, 160, 10, 0},
	{0xFF00FFFF, 200, 10, 0},
	{0xFF00FFFF, 200, 50, 0},
	{0xFF00FFFF, 160, 50, 0},
};

Vertex_C8888_P16 vertices5[4] = {
	{0xFFFF00FF, 210, 10, 0},
	{0xFFFF00FF, 250, 10, 0},
	{0xFFFF00FF, 250, 50, 0},
	{0xFFFF00FF, 210, 50, 0},
};

Vertex_C8888_P16 vertices6[4] = {
	{0xFFFFFF00, 260, 10, 0},
	{0xFFFFFF00, 300, 10, 0},
	{0xFFFF7F00, 300, 50, 0},
	{0xFFFF7F00, 260, 50, 0},
};

Vertex_C8888_P16 vertices7[2] = {
	{0xFFFFFFFF, 310, 10, 0},
	{0xFFFF00FF, 350, 50, 0},
};

Vertex_UV16_P16 vertices8[4] = {
	{0, 0, 360, 10, 0},
	{4, 0, 400, 10, 0},
	{4, 4, 400, 50, 0},
	{0, 4, 360, 50, 0},
};

Vertex_UV16_P16 vertices9[4] = {
	{0, 0, 410, 10, 0},
	{4, 0, 450, 10, 0},
	{4, 4, 450, 50, 0},
	{0, 4, 410, 50, 0},
};

// Clockwise
Vertex_C8888_P16 vertices10[4] = {
	{0xFFFFFF00, 10, 60, 0},
	{0xFFFFFF00, 50, 60, 0},
	{0xFFFF7F00, 50, 100, 0},
	{0xFFFF7F00, 10, 100, 0},
};
// Counter-clockwise
Vertex_C8888_P16 vertices11[4] = {
	{0xFFFFFF00, 60, 60, 0},
	{0xFFFFFF00, 60, 100, 0},
	{0xFFFFFF00, 100, 100, 0},
	{0xFFFFFF00, 100, 60, 0},
};

Vertex_C8888_P16 vertices12[4] = {
	{0xFFFFFF00, 110, 60, 0},
	{0xFFFFFF00, 150, 60, 0},
	{0xFFFF7F00, 150, 100, 0},
	{0xFFFF7F00, 110, 100, 0},
};

Vertex_C8888_P16 vertices13[4] = {
	{0xFFFFFF00, 160, 60, 0},
	{0xFFFFFF00, 200, 60, 0},
	{0xFFFF7F00, 200, 100, 0},
	{0xFFFF7F00, 160, 100, 0},
};

void sendImmediateVerts(int prim, int flags, int c, Vertex_C8888_P16 *v) {
	for (int i = 0; i < c; ++i) {
		sceGuSendCommandi(0xF0, 0x00FF0000 | ((2048 + v[i].x) << 4));
		sceGuSendCommandi(0xF1, 0x00FF0000 | ((2048 + v[i].y) << 4));
		sceGuSendCommandi(0xF2, 0x00FF0000 | v[i].z);
		// Color 0.
		sceGuSendCommandi(0xF6, v[i].c & 0x00FFFFFF);
		// Fog.
		sceGuSendCommandi(0xF8, 0x00FFFF7F);
		// Color 1.
		sceGuSendCommandi(0xF9, 0x007F7F7F);
		sceGuSendCommandi(0xF7, flags | (prim << 8) | (v[i].c >> 24));
	}
}

void sendImmediateVerts(int prim, int flags, int c, Vertex_UV16_P16 *v) {
	for (int i = 0; i < c; ++i) {
		sceGuSendCommandi(0xF0, 0x00FF0000 | ((2048 + v[i].x) << 4));
		sceGuSendCommandi(0xF1, 0x00FF0000 | ((2048 + v[i].y) << 4));
		sceGuSendCommandi(0xF2, 0x00FF0000 | v[i].z);
		sceGuSendCommandf(0xF3, v[i].u / 4.0f);
		sceGuSendCommandf(0xF4, v[i].v / 4.0f);
		sceGuSendCommandf(0xF5, 1.0f);

		// Color 0.
		sceGuSendCommandi(0xF6, 0x00FFFFFF);
		// Fog.
		sceGuSendCommandi(0xF8, 0x00FFFFFF);
		// Color 1.
		sceGuSendCommandi(0xF9, 0x007F7F7F);
		sceGuSendCommandi(0xF7, flags | (prim << 8) | 0xFF);
	}
}

void draw() {
	startFrame();
	sceGuDisable(GU_TEXTURE_2D);
	sceGuEnable(GU_BLEND);
	sceGuEnable(GU_DEPTH_TEST);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuDepthFunc(GU_ALWAYS);
	sceGuDepthMask(0);
	sceGuDepthOffset(0);
	sceGuDepthRange(0, 65535);
	// These don't matter, even if off they work.
	//sceGuEnable(GU_LIGHTING);
	//sceGuEnable(GU_FOG);
	sceGuLightMode(GU_SINGLE_COLOR);
	sceGuFog(0.0f, 1.0f, 0x00777777);

	sceGuViewport(0, 0, 1, 1);
	sceGuOffset(0, 0);

	sceGuClearColor(0x00000000);
	sceGuClearStencil(0x00);
	sceGuClearDepth(0);
	sceGuClear(GU_COLOR_BUFFER_BIT | GU_STENCIL_BUFFER_BIT);

	// The transform flag here is used (verts are not.)  Make sure to set after clear.
	sceGuSendCommandi(GE_CMD_VERTEXTYPE, GU_COLOR_5650 | GU_VERTEX_32BITF | GU_TRANSFORM_3D);

	// Translates only when vtype is non-through.
	sceGuSendCommandi(GE_CMD_OFFSETX, 2048 << 4);
	sceGuSendCommandi(GE_CMD_OFFSETY, 2048 << 4);

	sendImmediateVerts(GU_POINTS, 0, 1, vertices1);
	sendImmediateVerts(GU_CONTINUE, 0, 3, vertices1 + 1);

	sendImmediateVerts(GU_LINES, 0, 1, vertices2);
	sendImmediateVerts(GU_CONTINUE, 0, 3, vertices2 + 1);

	sendImmediateVerts(GU_LINE_STRIP, 0, 1, vertices3);
	sendImmediateVerts(GU_CONTINUE, 0, 3, vertices3 + 1);

	sendImmediateVerts(GU_TRIANGLES, 0, 1, vertices4);
	sendImmediateVerts(GU_CONTINUE, 0, 3, vertices4 + 1);

	sendImmediateVerts(GU_TRIANGLE_STRIP, 0, 1, vertices5);
	sendImmediateVerts(GU_CONTINUE, 0, 3, vertices5 + 1);

	sendImmediateVerts(GU_TRIANGLE_FAN, GU_IMM_SHADE_MODE, 1, vertices6);
	sendImmediateVerts(GU_CONTINUE, 0, 3, vertices6 + 1);

	sendImmediateVerts(GU_SPRITES, 0x00D40000, 1, vertices7);
	sendImmediateVerts(GU_CONTINUE, 0, 1, vertices7 + 1);

	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexFilter(GU_NEAREST, GU_NEAREST);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	sceGuTexMode(GU_PSM_8888, 0, 0, 0);
	sceGuTexImage(0, 4, 4, 4, imageData);
	sceGuTexWrap(GU_CLAMP, GU_CLAMP);
	sceGuTexFlush();
	sceGuTexSync();

	sendImmediateVerts(GU_TRIANGLE_FAN, GU_IMM_TEXTURE, 1, vertices8);
	sendImmediateVerts(GU_CONTINUE, 0, 3, vertices8 + 1);

	// Clear mode applies, including flags.
	sceGuSendCommandi(GE_CMD_CLEARMODE, 1 | ((GU_COLOR_BUFFER_BIT | GU_STENCIL_BUFFER_BIT | GU_DEPTH_BUFFER_BIT) << 8));
	sendImmediateVerts(GU_TRIANGLE_FAN, GU_IMM_TEXTURE, 1, vertices9);
	sendImmediateVerts(GU_CONTINUE, 0, 3, vertices9 + 1);
	sceGuSendCommandi(GE_CMD_CLEARMODE, 0);

	// Counter clockwise culled by GU_IMM_CULL | GU_IMM_CULL_MODE, fine all others.
	// Clockwise culled by GU_IMM_CULL, fine all others.
	sendImmediateVerts(GU_TRIANGLE_FAN, GU_IMM_CULL, 1, vertices10);
	sendImmediateVerts(GU_CONTINUE, 0, 3, vertices10 + 1);
	sendImmediateVerts(GU_TRIANGLE_FAN, GU_IMM_CULL | GU_IMM_CULL_MODE, 1, vertices11);
	sendImmediateVerts(GU_CONTINUE, 0, 3, vertices11 + 1);

	// Note: not done in throughmode.
	sceGuLightMode(GU_SEPARATE_SPECULAR_COLOR);
	sendImmediateVerts(GU_TRIANGLE_FAN, 0, 1, vertices12);
	sendImmediateVerts(GU_CONTINUE, 0, 3, vertices12 + 1);
	sceGuLightMode(GU_SINGLE_COLOR);

	sceGuLightMode(GU_SEPARATE_SPECULAR_COLOR);
	sceGuSendCommandi(GE_CMD_VERTEXTYPE, GU_COLOR_5650 | GU_VERTEX_32BITF | GU_TRANSFORM_2D);
	sendImmediateVerts(GU_TRIANGLE_FAN, 0, 1, vertices13);
	sendImmediateVerts(GU_CONTINUE, 0, 3, vertices13 + 1);
	sceGuLightMode(GU_SINGLE_COLOR);

	endFrame();
}

extern "C" int main(int argc, char *argv[]) {
	initDisplay();

	draw();

	emulatorEmitScreenshot();

	sceGuTerm();

	return 0;
}
