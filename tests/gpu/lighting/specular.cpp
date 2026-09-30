#include <cmath>
#include "boxes.h"

// Specular lighting as the GE computes it: pow(N.H, e) with H = normalize(L + V), and nothing when
// N.L < 0. V is the view matrix's third column (the viewer is at infinity along view space +z), so
// rotating the view moves the highlight. pow is exp2(e * log2(x)) with log2 and exp2 each a straight
// line between powers of two (Mitchell's approximation), for powered diffuse too, and e keeps only
// the top 4 bits of its mantissa: 5.1 acts as 5 and 0.3 as 0.296875.
//
// Values are printed as 4-wide ranges, and the normals are picked so each lands mid-range, so that
// a step of rounding difference doesn't matter (Mitchell and a true pow are 10-25 apart).

struct SpecCase {
	const char *section;
	const char *title;
	float e;
	int kind;
	float L[3];
	int viewRot;
	float N[3];
};

#include "specular_cases.h"

extern "C" int main(int argc, char *argv[]) {
	initDisplay();

	sceGuStart(GU_DIRECT, list);
	sceGuClearColor(0);
	sceGuClear(GU_COLOR_BUFFER_BIT);
	sceGuDisable(GU_TEXTURE_2D);
	sceGuDisable(GU_BLEND);
	sceGuDisable(GU_DITHER);
	sceGuEnable(GU_LIGHTING);
	sceGuEnable(GU_LIGHT0);
	sceGuAmbient(0xFF000000);
	sceGuLightMode(GU_SINGLE_COLOR);
	sceGuLightAtt(0, 1.0f, 0.0f, 0.0f);
	sceGuColorMaterial(0);
	sceGuModelColor(0x000000, 0x000000, 0xFFFFFF, 0xFFFFFF);

	const int count = sizeof(specCases) / sizeof(specCases[0]);
	for (int i = 0; i < count; ++i) {
		const SpecCase &c = specCases[i];
		ScePspFVector3 L = { c.L[0], c.L[1], c.L[2] };
		if (c.kind == 1) {
			sceGuLight(0, GU_DIRECTIONAL, GU_DIFFUSE_AND_SPECULAR, &L);
			sceGuLightColor(0, GU_DIFFUSE, 0x000000);
		} else {
			sceGuLight(0, GU_DIRECTIONAL, GU_POWERED_DIFFUSE, &L);
			sceGuLightColor(0, GU_DIFFUSE, 0xFFFFFF);
		}
		sceGuLightColor(0, GU_AMBIENT, 0x000000);
		sceGuLightColor(0, GU_SPECULAR, 0xFFFFFF);
		sceGuSpecular(c.e);
		setViewRotation(c.viewRot);
		drawBox(c.N);
	}

	sceGuFinish();
	sceGuSync(0, 0);
	readBoxes();

	const char *section = "";
	for (int i = 0; i < count; ++i) {
		const SpecCase &c = specCases[i];
		if (strcmp(section, c.section) != 0) {
			section = c.section;
			checkpointNext(section);
		}
		int lo = boxes[i].color & 0xFC;
		checkpoint("  %s, N=(%.3f, %.3f, %.3f): %d-%d", c.title, c.N[0], c.N[1], c.N[2], lo, lo + 3);
	}

	sceGuTerm();
	return 0;
}
