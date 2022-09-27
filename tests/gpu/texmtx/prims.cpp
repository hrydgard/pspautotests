#include "shared.h"

extern "C" int HAS_DISPLAY;

typedef struct {
	float nx, ny, nz;
	float x, y, z;
} VertexPosF32;

static __attribute__((aligned(16))) VertexPosF32 vertices_f32[32];
static __attribute__((aligned(16))) u32 texdata[256 * 256];

inline VertexPosF32 makeVertexPos32(float nx, float ny, float nz, float x, float y, float z) {
	VertexPosF32 v;
	v.nx = nx;
	v.ny = ny;
	v.nz = nz;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

void displayBuffer(const char *reason) {
	checkpoint("%s: TL=%08x TR=%08x BL=%08x BR=%08x", reason, readDispBuffer(0, 0), readDispBuffer(255, 0), readDispBuffer(0, 255), readDispBuffer(255, 255));
}

void init() {
	initDisplay();
	clearDispBuffer(0x44444444);
	displayBuffer("Initial");

	// Let's initialize the texture.
	for (int y = 0; y < 256; ++y) {
		for (int x = 0; x < 256; ++x) {
			texdata[y * 256 + x] = 0xFFFF0000 | (y << 8) | x;
		}
	}
	sceKernelDcacheWritebackInvalidateRange(texdata, sizeof(texdata));

	startFrame();
	sceGuTexImage(0, 256, 256, 256, texdata);
	sceGuTexFlush();
	sceGuTexSync();

	endFrame();
}

void testPrimType(const char *title, int prim) {
	startFrame();
	sceGuTexMapMode(GU_TEXTURE_MATRIX, 0, 0);
	sceGuTexProjMapMode(GU_NORMAL);

	if (prim == GU_POINTS || prim == GU_LINE_STRIP) {
		vertices_f32[0] = makeVertexPos32(0.0, 0.0, 1.0, 0.0, 0.0, 0.0);
		vertices_f32[1] = makeVertexPos32(0.5, 0.0, 1.0, 1.0f - 1.0f / 256.0f, 0.0, 0.0);
		vertices_f32[2] = makeVertexPos32(0.5, 0.5, 1.0, 1.0f - 1.0f / 256.0f, 1.0f - 1.0f / 256.0f, 0.0);
		vertices_f32[3] = makeVertexPos32(0.0, 0.5, 0.5, 0.0, 1.0f - 1.0f / 256.0f, 0.0);
	} else if (prim == GU_SPRITES) {
		vertices_f32[0] = makeVertexPos32(0.0, 0.0, 1.0, 0.0, 0.0, 0.0);
		vertices_f32[1] = makeVertexPos32(0.5, 1.0, 1.0, 0.5, 1.0, 0.0);
		vertices_f32[2] = makeVertexPos32(0.5, 0.0, 1.0, 0.5, 0.0, 0.0);
		vertices_f32[3] = makeVertexPos32(1.0, 1.0, 0.5, 1.0, 1.0, 0.0);
	} else {
		vertices_f32[0] = makeVertexPos32(0.0, 0.0, 1.0, 0.0, 0.0, 0.0);
		vertices_f32[1] = makeVertexPos32(1.0, 0.0, 1.0, 1.0, 0.0, 0.0);
		vertices_f32[2] = makeVertexPos32(1.0, 1.0, 1.0, 1.0, 1.0, 0.0);
		vertices_f32[3] = makeVertexPos32(0.0, 1.0, 0.5, 0.0, 1.0, 0.0);
	}

	sceKernelDcacheWritebackInvalidateRange(vertices_f32, sizeof(vertices_f32));
	sceGuDrawArray(prim, GU_NORMAL_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, NULL, vertices_f32);
	dirtyDispBuffer();

	endFrame();
	displayBuffer(title);
}

extern "C" int main(int argc, char *argv[]) {
	init();
	HAS_DISPLAY = 0;

	checkpointNext("Primitives:");
	testPrimType("  Points", GU_POINTS);
	testPrimType("  Lines", GU_LINE_STRIP);
	testPrimType("  Triangles", GU_TRIANGLE_FAN);
	testPrimType("  Rectangles", GU_SPRITES);
	emulatorEmitScreenshot();

	sceGuTerm();

	return 0;
}
