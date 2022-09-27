#include "shared.h"

extern "C" int HAS_DISPLAY;

typedef struct {
	float u, v;
	float nx, ny, nz;
	float x, y, z;
} VertexAllF32;

typedef struct {
	float x, y, z;
} VertexPosF32;

static __attribute__((aligned(16))) VertexAllF32 vertices_f32[32];
static __attribute__((aligned(16))) VertexPosF32 verticespos_f32[32];
static __attribute__((aligned(16))) u32 texdata[256 * 256];

static const ScePspFMatrix4 onesAddQ = {
	{1, 0, 0, 0},
	{0, 1, 0, 0},
	{0, 0, 1, 1},
	{0, 0, 1, 1},
};

inline VertexAllF32 makeVertexAll32(float uu, float vv, float nx, float ny, float nz, float x, float y, float z) {
	VertexAllF32 v;
	v.u = uu;
	v.v = vv;
	v.nx = nx;
	v.ny = ny;
	v.nz = nz;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

inline VertexPosF32 makeVertexPos32(float x, float y, float z) {
	VertexPosF32 v;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

void displayBuffer(const char *reason) {
	checkpoint("%s: TL=%08x TR=%08x BL=%08x BR=%08x", reason, readDispBuffer(0, 0), readDispBuffer(255, 0), readDispBuffer(0, 255), readDispBuffer(255, 255));
}

void drawBoxCommandsAll() {
	vertices_f32[0] = makeVertexAll32(0.25, 0.25, 0.0, 1.0, 1.0, 0.0, 0.0, -0.5);
	vertices_f32[1] = makeVertexAll32(0.75, 0.25, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0);
	vertices_f32[2] = makeVertexAll32(0.75, 0.75, 1.0, 0.0, 1.0, 1.0, 1.0, 0.0);
	vertices_f32[3] = makeVertexAll32(0.25, 0.75, 0.0, 0.0, 0.5, 0.0, 1.0, 0.0);

	sceKernelDcacheWritebackInvalidateRange(vertices_f32, sizeof(vertices_f32));
	sceGuDrawArray(GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_NORMAL_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, NULL, vertices_f32);
	dirtyDispBuffer();
}

void drawBoxCommandsPos() {
	// First, let's send a prim to fill the verts.
	vertices_f32[0] = makeVertexAll32(0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0);
	vertices_f32[1] = makeVertexAll32(0.75, 0.25, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0);

	verticespos_f32[0] = makeVertexPos32(0.0, 0.0, -0.5);
	verticespos_f32[1] = makeVertexPos32(1.0, 0.0, 0.0);
	verticespos_f32[2] = makeVertexPos32(1.0, 1.0, 0.0);
	verticespos_f32[3] = makeVertexPos32(0.0, 1.0, 0.0);

	sceKernelDcacheWritebackInvalidateRange(vertices_f32, sizeof(vertices_f32));
	sceKernelDcacheWritebackInvalidateRange(verticespos_f32, sizeof(verticespos_f32));

	sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_NORMAL_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 2, NULL, vertices_f32);
	sceGuDrawArray(GU_TRIANGLE_FAN, GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, NULL, verticespos_f32);
	dirtyDispBuffer();
}

void drawBoxCommandsThrough() {
	vertices_f32[0] = makeVertexAll32(64.0f, 64.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, -32767.0f);
	vertices_f32[1] = makeVertexAll32(192.0f, 64.0f, 1.0f, 1.0f, 1.0f, 256.0f, 0.0f, 0.0f);
	vertices_f32[2] = makeVertexAll32(192.0f, 192.0f, 1.0f, 0.0f, 1.0f, 256.0f, 256.0f, 0.0f);
	vertices_f32[3] = makeVertexAll32(64.0f, 192.0f, 0.0f, 0.0f, 0.5f, 0.0f, 256.0f, 0.0f);

	sceKernelDcacheWritebackInvalidateRange(vertices_f32, sizeof(vertices_f32));
	sceGuDrawArray(GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_NORMAL_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 4, NULL, vertices_f32);
	dirtyDispBuffer();
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

	// Set Z to always be +1, so we can verify UV handling.
	sceGuSetMatrix(GU_TEXTURE, &onesAddQ);

	endFrame();
}

void testSource(const char *title, int mode, int type) {
	startFrame();
	sceGuTexMapMode(GU_TEXTURE_MATRIX, 0, 0);
	sceGuTexProjMapMode(mode);
	sceGuShadeModel(type == 2 ? GU_FLAT : GU_SMOOTH);

	if (type == 0 || type == 2) {
		drawBoxCommandsAll();
	} else if (type == 3) {
		drawBoxCommandsThrough();
	} else {
		drawBoxCommandsPos();
	}

	endFrame();
	displayBuffer(title);
}

extern "C" int main(int argc, char *argv[]) {
	init();
	HAS_DISPLAY = 0;

	checkpointNext("Common:");
	testSource("  Position", GU_POSITION, 0);
	emulatorEmitScreenshot();
	testSource("  UV", GU_UV, 0);
	testSource("  Normal", GU_NORMAL, 0);
	testSource("  Normalized normal", GU_NORMALIZED_NORMAL, 0);
	testSource("  Unknown", 3, 0);

	checkpointNext("Without UV / normal:");
	testSource("  Position", GU_POSITION, 1);
	testSource("  UV", GU_UV, 1);
	testSource("  Normal", GU_NORMAL, 1);
	testSource("  Normalized normal", GU_NORMALIZED_NORMAL, 1);
	testSource("  Unknown", 3, 1);

	checkpointNext("Flat:");
	testSource("  Position", GU_POSITION, 2);
	testSource("  UV", GU_UV, 2);
	testSource("  Normal", GU_NORMAL, 2);
	testSource("  Normalized normal", GU_NORMALIZED_NORMAL, 2);
	testSource("  Unknown", 3, 2);

	checkpointNext("Through:");
	testSource("  Position", GU_POSITION, 3);
	testSource("  UV", GU_UV, 3);
	testSource("  Normal", GU_NORMAL, 3);
	testSource("  Normalized normal", GU_NORMALIZED_NORMAL, 3);
	testSource("  Unknown", 3, 3);

	sceGuTerm();

	return 0;
}
