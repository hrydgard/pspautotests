#include "shared.h"
#include <psppower.h>

typedef struct {
	unsigned int *start;
	unsigned int *current;
	int parent_context;
} GuDisplayList;

typedef struct {
	float x, y, z;
} VertexF32;

extern GuDisplayList *gu_list;

enum TestSide {
	TEST_X,
	TEST_Y,
	TEST_Z,
	TEST_W,
};

static void testVerts(const char *title, TestSide side, float value, bool clamp = true) {
	startFrame();
	VertexF32 *verts = (VertexF32 *)sceGuGetMemory(12);
	verts->x = 1.0f;
	verts->y = 1.0f;
	verts->z = 1.0f;

	ScePspMatrix4 projMatrix = {{
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{0, 0, 0, 1},
	}};

	// Adjust the projection matrix to set the value.
	if (value != 1.0f) {
		projMatrix.f[side][side] = value;
	}

	sceGuSetMatrix(GU_PROJECTION, &projMatrix.fm);
	sceGuSetStatus(GU_CLIP_PLANES, clamp);

	sceGuSendCommandi(GE_CMD_AMBIENTALPHA, 1);
	sceGuSendCommandi(GE_CMD_VERTEXTYPE, GU_VERTEX_32BITF | GU_TRANSFORM_3D);
	sceGuSendCommandi(GE_CMD_BASE, (((uintptr_t)verts) >> 8) & 0x00FF0000);
	sceGuSendCommandi(GE_CMD_VADDR, ((uintptr_t)verts) & 0x00FFFFFF);
	sceGuSendCommandi(GE_CMD_BOUNDINGBOX, 1);
	sceGuSendCommandi(GE_CMD_BJUMP, ((uintptr_t)gu_list->current + 8) & 0x00FFFFFF);
	sceGuSendCommandi(GE_CMD_AMBIENTALPHA, 2);
	sceGuSendCommandi(GE_CMD_NOP, 0);

	endFrame();

	checkpoint("%s: %08x", title, sceGeGetCmd(GE_CMD_AMBIENTALPHA));
}

extern "C" int main(int argc, char *argv[]) {
	initDisplay();
	scePowerSetClockFrequency(333, 333, 166);

	checkpointNext("X:");
	testVerts("  X outside negative", TEST_X, -1.01f);
	testVerts("  X inside negative", TEST_X, -1.0f);
	testVerts("  X inside positive", TEST_X, 1.0f);
	testVerts("  X outside positive", TEST_X, 1.01f);

	checkpointNext("Y:");
	testVerts("  Y outside negative", TEST_Y, -1.01f);
	testVerts("  Y inside negative", TEST_Y, -1.0f);
	testVerts("  Y inside positive", TEST_Y, 1.0f);
	testVerts("  Y outside positive", TEST_Y, 1.01f);

	checkpointNext("Z:");
	testVerts("  Z outside negative, clamp", TEST_Z, -1.01f, true);
	testVerts("  Z inside negative, clamp", TEST_Z, -1.0f, true);
	testVerts("  Z inside positive, clamp", TEST_Z, 1.0f, true);
	testVerts("  Z outside positive, clamp", TEST_Z, 1.01f, true);
	testVerts("  Z inside very negative, noclamp", TEST_Z, -65535.999f, false);
	testVerts("  Z inside negative, noclamp", TEST_Z, -1.0f, false);
	testVerts("  Z inside positive, noclamp", TEST_Z, 1.0f, false);
	testVerts("  Z inside very positive, noclamp", TEST_Z, 65535.999f, false);

	checkpointNext("W:");
	testVerts("  W outside negative", TEST_W, -0.01f);
	testVerts("  W inside negative", TEST_W, -0.0f);
	testVerts("  W inside positive", TEST_W, 1.0f);
	testVerts("  W inside very positive", TEST_W, 65535.999f);

	sceGuTerm();
	return 0;
}
