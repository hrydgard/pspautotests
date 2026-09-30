#include <cmath>
#include "boxes.h"

// Environment map (shade mapping) texture coordinates. S comes from light 0 and T from light 1, each
// (N.L + 1) / 2 with L the light's vector as lighting sees it. A zero vector stays zero. For a light
// that does specular, L is replaced by the half vector normalize(L + V), with V the view matrix's
// third column. Lighting and light enables don't matter.
//
// A texture whose texels hold their own coordinates shows S and T. The normals are picked so both are
// mid-texel, away from rounding differences.

struct ShadeCase {
	const char *title;
	int kind;
	float L0[3];
	float L1[3];
	int lighting;
	int enabled;
	int viewRot;
	float N[3];
};

#include "shademap_cases.h"

static u32 __attribute__((aligned(16))) coordTexture[256 * 256];

extern "C" int main(int argc, char *argv[]) {
	initDisplay();

	for (int v = 0; v < 256; ++v) {
		for (int u = 0; u < 256; ++u) {
			coordTexture[v * 256 + u] = 0xFF000000 | (v << 8) | u;
		}
	}
	sceKernelDcacheWritebackAll();

	sceGuStart(GU_DIRECT, list);
	sceGuClearColor(0);
	sceGuClear(GU_COLOR_BUFFER_BIT);
	sceGuDisable(GU_BLEND);
	sceGuDisable(GU_DITHER);
	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexMode(GU_PSM_8888, 0, 0, 0);
	sceGuTexImage(0, 256, 256, 256, coordTexture);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	sceGuTexFilter(GU_NEAREST, GU_NEAREST);
	sceGuTexWrap(GU_CLAMP, GU_CLAMP);
	sceGuTexMapMode(GU_ENVIRONMENT_MAP, 0, 1);
	sceGuLightAtt(0, 1.0f, 0.0f, 0.0f);
	sceGuLightAtt(1, 1.0f, 0.0f, 0.0f);

	static const int components[3] = { GU_AMBIENT_AND_DIFFUSE, GU_DIFFUSE_AND_SPECULAR, GU_POWERED_DIFFUSE };
	const int count = sizeof(shadeCases) / sizeof(shadeCases[0]);
	for (int i = 0; i < count; ++i) {
		const ShadeCase &c = shadeCases[i];
		ScePspFVector3 L0 = { c.L0[0], c.L0[1], c.L0[2] };
		ScePspFVector3 L1 = { c.L1[0], c.L1[1], c.L1[2] };
		sceGuLight(0, GU_DIRECTIONAL, components[c.kind], &L0);
		sceGuLight(1, GU_DIRECTIONAL, components[c.kind], &L1);
		if (c.lighting) {
			sceGuEnable(GU_LIGHTING);
		} else {
			sceGuDisable(GU_LIGHTING);
		}
		if (c.enabled) {
			sceGuEnable(GU_LIGHT0);
			sceGuEnable(GU_LIGHT1);
		} else {
			sceGuDisable(GU_LIGHT0);
			sceGuDisable(GU_LIGHT1);
		}
		setViewRotation(c.viewRot);
		drawBox(c.N);
	}

	sceGuFinish();
	sceGuSync(0, 0);
	readBoxes();

	const char *title = "";
	for (int i = 0; i < count; ++i) {
		const ShadeCase &c = shadeCases[i];
		if (strcmp(title, c.title) != 0) {
			title = c.title;
			checkpointNext(title);
		}
		checkpoint("  N=(%.3f, %.3f, %.3f): S %d, T %d", c.N[0], c.N[1], c.N[2], boxes[i].color & 0xFF, (boxes[i].color >> 8) & 0xFF);
	}

	sceGuTerm();
	return 0;
}
