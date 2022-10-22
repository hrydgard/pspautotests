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

static const char *getResult() {
	switch (sceGeGetCmd(GE_CMD_AMBIENTALPHA)) {
	case 0x5D000002:
		return "inside";
	case 0x5D000001:
		return "outside";
	default:
		break;
	}
	return "testerror";
}

static void testVertsMinMaxZ(const char *title, float value, uint16_t minZ, uint16_t maxZ, bool clamp = true) {
	startFrame();
	VertexF32 *verts = (VertexF32 *)sceGuGetMemory(12);
	verts->x = 1.0f;
	verts->y = 1.0f;
	verts->z = 1.0f;

	ScePspMatrix4 projMatrix = { {
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{0, 0, 0, 1},
	} };

	// Adjust the projection matrix to set the value.
	if (value != 1.0f) {
		projMatrix.f[TEST_Z][TEST_Z] = value;
	}

	sceGuSetMatrix(GU_PROJECTION, &projMatrix.fm);
	sceGuSetStatus(GU_CLIP_PLANES, clamp);
	sceGuSendCommandi(GE_CMD_MINZ, minZ);
	sceGuSendCommandi(GE_CMD_MAXZ, maxZ);

	sceGuSendCommandi(GE_CMD_OFFSETX, (2048 - 240) << 4);
	sceGuSendCommandi(GE_CMD_OFFSETY, (2048 - 136) << 4);
	sceGuSendCommandf(GE_CMD_VIEWPORTX1, 240.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTY1, -136.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTZ1, -32767.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTX2, 2048.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTY2, 2048.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTZ2, 32767.0f);

	sceGuSendCommandi(GE_CMD_SCISSOR1, (0 << 10) | 0);
	sceGuSendCommandi(GE_CMD_SCISSOR2, (271 << 10) | 479);
	sceGuSendCommandi(GE_CMD_REGION1, (0 << 10) | 0);
	sceGuSendCommandi(GE_CMD_REGION2, (271 << 10) | 479);

	sceGuSendCommandi(GE_CMD_AMBIENTALPHA, 1);
	sceGuSendCommandi(GE_CMD_VERTEXTYPE, GU_VERTEX_32BITF | GU_TRANSFORM_3D);
	sceGuSendCommandi(GE_CMD_BASE, (((uintptr_t)verts) >> 8) & 0x00FF0000);
	sceGuSendCommandi(GE_CMD_VADDR, ((uintptr_t)verts) & 0x00FFFFFF);
	sceGuSendCommandi(GE_CMD_BOUNDINGBOX, 1);
	sceGuSendCommandi(GE_CMD_BJUMP, ((uintptr_t)gu_list->current + 8) & 0x00FFFFFF);
	sceGuSendCommandi(GE_CMD_AMBIENTALPHA, 2);
	sceGuSendCommandi(GE_CMD_NOP, 0);

	endFrame();

	checkpoint("%s: %s", title, getResult());
}

static void testVertsScissor(const char *title, TestSide side, float value, uint16_t scissor1, uint16_t scissor2) {
	startFrame();
	VertexF32 *verts = (VertexF32 *)sceGuGetMemory(12);
	verts->x = 1.0f;
	verts->y = 1.0f;
	verts->z = 1.0f;

	ScePspMatrix4 projMatrix = { {
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{0, 0, 0, 1},
	} };

	// Adjust the projection matrix to set the value.
	if (value != 1.0f) {
		projMatrix.f[side][side] = value;
	}

	sceGuSetMatrix(GU_PROJECTION, &projMatrix.fm);
	sceGuSetStatus(GU_CLIP_PLANES, false);
	sceGuSendCommandi(GE_CMD_MINZ, 0x0000);
	sceGuSendCommandi(GE_CMD_MAXZ, 0xFFFF);

	sceGuSendCommandi(GE_CMD_OFFSETX, (2048 - 240) << 4);
	sceGuSendCommandi(GE_CMD_OFFSETY, (2048 - 136) << 4);
	sceGuSendCommandf(GE_CMD_VIEWPORTX1, 240.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTY1, -136.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTZ1, -32767.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTX2, 2048.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTY2, 2048.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTZ2, 32767.0f);

	sceGuSendCommandi(GE_CMD_SCISSOR1, ((side == TEST_X ? 0 : scissor1) << 10) | (side == TEST_X ? scissor1 : 0));
	sceGuSendCommandi(GE_CMD_SCISSOR2, ((side == TEST_X ? 271 : scissor2) << 10) | (side == TEST_X ? scissor2 : 479));
	sceGuSendCommandi(GE_CMD_REGION1, (0 << 10) | 0);
	sceGuSendCommandi(GE_CMD_REGION2, (271 << 10) | 479);

	sceGuSendCommandi(GE_CMD_AMBIENTALPHA, 1);
	sceGuSendCommandi(GE_CMD_VERTEXTYPE, GU_VERTEX_32BITF | GU_TRANSFORM_3D);
	sceGuSendCommandi(GE_CMD_BASE, (((uintptr_t)verts) >> 8) & 0x00FF0000);
	sceGuSendCommandi(GE_CMD_VADDR, ((uintptr_t)verts) & 0x00FFFFFF);
	sceGuSendCommandi(GE_CMD_BOUNDINGBOX, 1);
	sceGuSendCommandi(GE_CMD_BJUMP, ((uintptr_t)gu_list->current + 8) & 0x00FFFFFF);
	sceGuSendCommandi(GE_CMD_AMBIENTALPHA, 2);
	sceGuSendCommandi(GE_CMD_NOP, 0);

	endFrame();

	checkpoint("%s: %s", title, getResult());
}

static void testVertsRegion(const char *title, TestSide side, float value, uint16_t scissor1, uint16_t scissor2) {
	startFrame();
	VertexF32 *verts = (VertexF32 *)sceGuGetMemory(12);
	verts->x = 1.0f;
	verts->y = 1.0f;
	verts->z = 1.0f;

	ScePspMatrix4 projMatrix = { {
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{0, 0, 0, 1},
	} };

	// Adjust the projection matrix to set the value.
	if (value != 1.0f) {
		projMatrix.f[side][side] = value;
	}

	sceGuSetMatrix(GU_PROJECTION, &projMatrix.fm);
	sceGuSetStatus(GU_CLIP_PLANES, false);
	sceGuSendCommandi(GE_CMD_MINZ, 0x0000);
	sceGuSendCommandi(GE_CMD_MAXZ, 0xFFFF);

	sceGuSendCommandi(GE_CMD_OFFSETX, (2048 - 240) << 4);
	sceGuSendCommandi(GE_CMD_OFFSETY, (2048 - 136) << 4);
	sceGuSendCommandf(GE_CMD_VIEWPORTX1, 240.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTY1, -136.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTZ1, -32767.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTX2, 2048.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTY2, 2048.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTZ2, 32767.0f);

	sceGuSendCommandi(GE_CMD_SCISSOR1, (0 << 10) | 0);
	sceGuSendCommandi(GE_CMD_SCISSOR2, (271 << 10) | 479);
	sceGuSendCommandi(GE_CMD_REGION1, ((side == TEST_X ? 0 : scissor1) << 10) | (side == TEST_X ? scissor1 : 0));
	sceGuSendCommandi(GE_CMD_REGION2, ((side == TEST_X ? 271 : scissor2) << 10) | (side == TEST_X ? scissor2 : 479));

	sceGuSendCommandi(GE_CMD_AMBIENTALPHA, 1);
	sceGuSendCommandi(GE_CMD_VERTEXTYPE, GU_VERTEX_32BITF | GU_TRANSFORM_3D);
	sceGuSendCommandi(GE_CMD_BASE, (((uintptr_t)verts) >> 8) & 0x00FF0000);
	sceGuSendCommandi(GE_CMD_VADDR, ((uintptr_t)verts) & 0x00FFFFFF);
	sceGuSendCommandi(GE_CMD_BOUNDINGBOX, 1);
	sceGuSendCommandi(GE_CMD_BJUMP, ((uintptr_t)gu_list->current + 8) & 0x00FFFFFF);
	sceGuSendCommandi(GE_CMD_AMBIENTALPHA, 2);
	sceGuSendCommandi(GE_CMD_NOP, 0);

	endFrame();

	checkpoint("%s: %s", title, getResult());
}

static void testVertsViewport(const char *title, TestSide side, float value, float scale, float center, uint16_t offset, bool clamp = false) {
	startFrame();
	VertexF32 *verts = (VertexF32 *)sceGuGetMemory(12);
	verts->x = 1.0f;
	verts->y = 1.0f;
	verts->z = 1.0f;

	ScePspMatrix4 projMatrix = { {
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{0, 0, 0, 1},
	} };

	// Adjust the projection matrix to set the value.
	if (value != 1.0f) {
		projMatrix.f[side][side] = value;
	}

	sceGuSetMatrix(GU_PROJECTION, &projMatrix.fm);
	sceGuSetStatus(GU_CLIP_PLANES, clamp);
	sceGuSendCommandi(GE_CMD_MINZ, 0x0000);
	sceGuSendCommandi(GE_CMD_MAXZ, 0xFFFF);

	if ((offset < 0 || offset >= 4096) && side < TEST_Z) {
		printf("TESTERROR: Invalid offset=%08x (%s)\n", offset << 4, title);
	}

	sceGuSendCommandi(GE_CMD_OFFSETX, (side == TEST_X ? offset : 2048 - 240) << 4);
	sceGuSendCommandi(GE_CMD_OFFSETY, (side == TEST_Y ? offset : 2048 - 136) << 4);
	sceGuSendCommandf(GE_CMD_VIEWPORTX1, side == TEST_X ? scale : 240.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTY1, side == TEST_Y ? scale : -136.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTZ1, side == TEST_Z ? scale : -32767.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTX2, side == TEST_X ? center : 2048.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTY2, side == TEST_Y ? center : 2048.0f);
	sceGuSendCommandf(GE_CMD_VIEWPORTZ2, side == TEST_Z ? center : 32767.0f);

	sceGuSendCommandi(GE_CMD_SCISSOR1, (0 << 10) | 0);
	sceGuSendCommandi(GE_CMD_SCISSOR2, (271 << 10) | 479);
	sceGuSendCommandi(GE_CMD_REGION1, (0 << 10) | 0);
	sceGuSendCommandi(GE_CMD_REGION2, (271 << 10) | 479);

	sceGuSendCommandi(GE_CMD_AMBIENTALPHA, 1);
	sceGuSendCommandi(GE_CMD_VERTEXTYPE, GU_VERTEX_32BITF | GU_TRANSFORM_3D);
	sceGuSendCommandi(GE_CMD_BASE, (((uintptr_t)verts) >> 8) & 0x00FF0000);
	sceGuSendCommandi(GE_CMD_VADDR, ((uintptr_t)verts) & 0x00FFFFFF);
	sceGuSendCommandi(GE_CMD_BOUNDINGBOX, 1);
	sceGuSendCommandi(GE_CMD_BJUMP, ((uintptr_t)gu_list->current + 8) & 0x00FFFFFF);
	sceGuSendCommandi(GE_CMD_AMBIENTALPHA, 2);
	sceGuSendCommandi(GE_CMD_NOP, 0);

	endFrame();

	checkpoint("%s: %s", title, getResult());
}

extern "C" int main(int argc, char *argv[]) {
	initDisplay();
	scePowerSetClockFrequency(333, 333, 166);

	checkpointNext("Z - min/max test:");
	testVertsMinMaxZ("  Z outside min, clamp", 1.0f, 0xFFFF, 0xFFFF, true);
	testVertsMinMaxZ("  Z outside min, noclamp", 1.0f, 0xFFFF, 0xFFFF, false);
	testVertsMinMaxZ("  Z inside max, clamp", 1.0f, 0, 0, true);
	testVertsMinMaxZ("  Z inside max, noclamp", 1.0f, 0, 0, false);

	checkpointNext("Viewport Z:");
	testVertsViewport("  Z inside range, outside projected -w, clamp", TEST_Z, -2.0f, 16383.0f, 32767.0f, 0, true);
	testVertsViewport("  Z outside range, outside projected -w, clamp", TEST_Z, -3.0f, 16383.0f, 32767.0f, 0, true);
	testVertsViewport("  Z inside range, outside projected -w, noclamp", TEST_Z, -2.0f, 16383.0f, 32767.0f, 0, false);
	testVertsViewport("  Z outside range, outside projected -w, noclamp", TEST_Z, -3.0f, 16383.0f, 32767.0f, 0, false);
	testVertsViewport("  Z inside range, outside projected +w, clamp", TEST_Z, 2.0f, 16383.0f, 32767.0f, 0, true);
	testVertsViewport("  Z outside range, outside projected +w, clamp", TEST_Z, 3.0f, 16383.0f, 32767.0f, 0, true);
	testVertsViewport("  Z inside range, outside projected +w, noclamp", TEST_Z, 2.0f, 16383.0f, 32767.0f, 0, false);
	testVertsViewport("  Z outside range, outside projected +w, noclamp", TEST_Z, 3.0f, 16383.0f, 32767.0f, 0, false);

	checkpointNext("Scissor:");
	testVertsScissor("  X outside scissor negative", TEST_X, -1.0f, 240, 479);
	testVertsScissor("  X inside scissor negative", TEST_X, -1.0f, 0, 0);
	testVertsScissor("  X inside scissor positive", TEST_X, 1.0f, 240, 479);
	testVertsScissor("  X outside scissor positive", TEST_X, 0.1f, 0, 0);
	testVertsScissor("  Y outside scissor negative", TEST_Y, -1.0f, 0, 135);
	testVertsScissor("  Y inside scissor negative", TEST_Y, -1.0f, 136, 271);
	testVertsScissor("  Y inside scissor positive", TEST_Y, 1.0f, 0, 135);
	testVertsScissor("  Y outside scissor positive", TEST_Y, 0.1f, 271, 271);

	// Note: region 1, in drawing, is not treated as a min coordinate but seems to be here.
	checkpointNext("Region:");
	testVertsRegion("  X outside region negative", TEST_X, -1.0f, 240, 479);
	testVertsRegion("  X inside region negative", TEST_X, -1.0f, 0, 0);
	testVertsRegion("  X inside region positive", TEST_X, 1.0f, 240, 479);
	testVertsRegion("  X outside region positive", TEST_X, 0.1f, 0, 0);
	testVertsRegion("  Y outside region negative", TEST_Y, -1.0f, 0, 135);
	testVertsRegion("  Y inside region negative", TEST_Y, -1.0f, 136, 271);
	testVertsRegion("  Y inside region positive", TEST_Y, 1.0f, 0, 135);
	testVertsRegion("  Y outside region positive", TEST_Y, 0.1f, 271, 271);

	checkpointNext("Viewport X:");
	testVertsViewport("  X outside range, outside projected -w", TEST_X, -3.0f, 120.0f, 2048.0f, 2048 - 240);
	testVertsViewport("  X inside range, outside projected -w", TEST_X, -2.0f, 120.0f, 2048.0f, 2048 - 240);
	testVertsViewport("  X outside range, outside projected +w", TEST_X, 3.0f, 120.0f, 2048.0f, 2048 - 240);
	testVertsViewport("  X inside range, outside projected +w", TEST_X, 2.0f, 120.0f, 2048.0f, 2048 - 240);

	checkpointNext("Viewport Y:");
	testVertsViewport("  Y outside range, outside projected -w", TEST_Y, -3.0f, 68.0f, 2048.0f, 2048 - 136);
	testVertsViewport("  Y inside range, outside projected -w", TEST_Y, -2.0f, 68.0f, 2048.0f, 2048 - 136);
	testVertsViewport("  Y outside range, outside projected +w", TEST_Y, 3.0f, 68.0f, 2048.0f, 2048 - 136);
	testVertsViewport("  Y inside range, outside projected +w", TEST_Y, 2.0f, 68.0f, 2048.0f, 2048 - 136);

	checkpointNext("Cullbox X");
	testVertsViewport("  X outside negative, cullbox offset=0", TEST_X, -999.0f, 120.0f, 0.0f, 0);
	testVertsViewport("  X inside negative, cullbox offset=0", TEST_X, -0.0f, 120.0f, 0.0f, 0);
	testVertsViewport("  X outside negative, cullbox offset=2", TEST_X, -2.0f, 1.0f, 2.0f, 2);
	testVertsViewport("  X inside negative, cullbox offset=2", TEST_X, -1.0f, 1.0f, 2.0f, 2);
	testVertsViewport("  X outside positive, cullbox offset=4095", TEST_X, 999.0f, 1.0f, 4095.0f, 4095);
	testVertsViewport("  X inside positive, cullbox offset=4095", TEST_X, 1.0f, 1.0f, 4095.0f, 4095);
	testVertsViewport("  X outside positive, cullbox offset=3615", TEST_X, 999.0f, 1.0f, 4095.0f, 3615);
	testVertsViewport("  X outside positive, cullbox offset=3616", TEST_X, 999.0f, 1.0f, 4095.0f, 3616);

	checkpointNext("Cullbox Y");
	testVertsViewport("  Y outside negative, cullbox offset=0", TEST_Y, -999.0f, 68.0f, 0.0f, 0);
	testVertsViewport("  Y inside negative, cullbox offset=0", TEST_Y, -0.0f, 68.0f, 0.0f, 0);
	testVertsViewport("  Y outside negative, cullbox offset=2", TEST_Y, -2.0f, 1.0f, 2.0f, 2);
	testVertsViewport("  Y inside negative, cullbox offset=2", TEST_Y, -1.0f, 1.0f, 2.0f, 2);
	testVertsViewport("  Y outside positive, cullbox offset=4095", TEST_Y, 999.0f, 1.0f, 4095.0f, 4095);
	testVertsViewport("  Y inside positive, cullbox offset=4095", TEST_Y, 1.0f, 1.0f, 4095.0f, 4095);
	testVertsViewport("  Y outside positive, cullbox offset=3823", TEST_Y, 999.0f, 1.0f, 4095.0f, 3823);
	testVertsViewport("  Y outside positive, cullbox offset=3824", TEST_Y, 999.0f, 1.0f, 4095.0f, 3824);

	sceGuTerm();
	return 0;
}
