#include "shared.h"

extern "C" int HAS_DISPLAY;

// What the GE samples when the projected q (from the texture matrix) isn't positive.
// The texture matrix makes s and t constant and q linear in u, so q changes sign across the quad.
// Each texel has its own color, so a division by a negative q, or a fixed texel, is easy to tell apart.

typedef struct {
	float u, v;
	float x, y, z;
} VertexUVF32;

static __attribute__((aligned(16))) VertexUVF32 vertices[4];
static __attribute__((aligned(16))) u32 texdata[256 * 256];

static VertexUVF32 makeVertex(float u, float v, float x, float y, float z) {
	VertexUVF32 vert;
	vert.u = u;
	vert.v = v;
	vert.x = x;
	vert.y = y;
	vert.z = z;
	return vert;
}

static void drawQuad(float s, float t, float qAtU0, float qPerU, int wrap) {
	// Rows are the coefficients of u, v, z and the constant, for s, t and q.
	ScePspFMatrix4 texmtx = {
		{0, 0, qPerU, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0},
		{s, t, qAtU0, 0},
	};

	startFrame();
	sceGuSetMatrix(GU_TEXTURE, &texmtx);
	sceGuTexMapMode(GU_TEXTURE_MATRIX, 0, 0);
	sceGuTexProjMapMode(GU_UV);
	sceGuTexWrap(wrap, wrap);

	vertices[0] = makeVertex(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	vertices[1] = makeVertex(1.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	vertices[2] = makeVertex(1.0f, 1.0f, 1.0f, 1.0f, 0.0f);
	vertices[3] = makeVertex(0.0f, 1.0f, 0.0f, 1.0f, 0.0f);
	sceKernelDcacheWritebackInvalidateRange(vertices, sizeof(vertices));
	sceGuDrawArray(GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, NULL, vertices);
	dirtyDispBuffer();
	endFrame();
}

static void report(const char *title, float qAtU0, float qPerU) {
	static const int xs[] = { 8, 40, 72, 104, 120, 126, 127, 128, 129, 130, 136, 152, 184, 216, 248 };
	checkpointNext(title);
	for (size_t i = 0; i < sizeof(xs) / sizeof(xs[0]); ++i) {
		// u at the pixel center is (x + 0.5) / 256.
		checkpoint("  x=%3d q=%+.4f: %08x", xs[i], qAtU0 + qPerU * (xs[i] + 0.5f) / 256.0f, readDispBuffer(xs[i], 128));
	}
}

extern "C" int main(int argc, char *argv[]) {
	initDisplay();
	clearDispBuffer(0x44444444);

	for (int y = 0; y < 256; ++y) {
		for (int x = 0; x < 256; ++x) {
			texdata[y * 256 + x] = 0xFF000000 | (y << 8) | x;
		}
	}
	sceKernelDcacheWritebackInvalidateRange(texdata, sizeof(texdata));

	startFrame();
	sceGuTexImage(0, 256, 256, 256, texdata);
	sceGuTexFlush();
	sceGuTexSync();
	endFrame();

	HAS_DISPLAY = 0;

	drawQuad(0.3f, 0.7f, -0.5f, 1.0f, GU_REPEAT);
	report("Repeat, q = u - 0.5:", -0.5f, 1.0f);

	// Same with q decreasing, so the negative side is on the right.
	clearDispBuffer(0x44444444);
	drawQuad(0.3f, 0.7f, 0.5f, -1.0f, GU_REPEAT);
	report("Repeat, q = 0.5 - u:", 0.5f, -1.0f);

	clearDispBuffer(0x44444444);
	drawQuad(0.3f, 0.7f, -0.5f, 1.0f, GU_CLAMP);
	report("Clamp, q = u - 0.5:", -0.5f, 1.0f);

	// The sign of s and t where q is negative.
	clearDispBuffer(0x44444444);
	drawQuad(-0.3f, 0.7f, -0.5f, 1.0f, GU_REPEAT);
	report("Repeat, s negative, q = u - 0.5:", -0.5f, 1.0f);

	clearDispBuffer(0x44444444);
	drawQuad(0.3f, -0.7f, -0.5f, 1.0f, GU_REPEAT);
	report("Repeat, t negative, q = u - 0.5:", -0.5f, 1.0f);

	clearDispBuffer(0x44444444);
	drawQuad(0.0f, 0.0f, -0.5f, 1.0f, GU_REPEAT);
	report("Repeat, s and t zero, q = u - 0.5:", -0.5f, 1.0f);

	// q exactly 0 everywhere.
	clearDispBuffer(0x44444444);
	drawQuad(0.3f, 0.7f, 0.0f, 0.0f, GU_REPEAT);
	report("Repeat, q = 0:", 0.0f, 0.0f);

	clearDispBuffer(0x44444444);
	drawQuad(0.0f, 0.0f, 0.0f, 0.0f, GU_REPEAT);
	report("Repeat, s, t and q zero:", 0.0f, 0.0f);

	clearDispBuffer(0x44444444);
	drawQuad(-0.3f, -0.7f, 0.0f, 0.0f, GU_CLAMP);
	report("Clamp, s and t negative, q = 0:", 0.0f, 0.0f);

	emulatorEmitScreenshot();
	sceGuTerm();
	return 0;
}
