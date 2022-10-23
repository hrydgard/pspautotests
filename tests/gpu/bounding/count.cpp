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

static void testVerts(const char *title, int c, bool allInside, int iSwap = -1, int swapc = 1) {
	startFrame();
	VertexF32 *verts = (VertexF32 *)sceGuGetMemory(c * 12);
	for (int i = 0; i < c; ++i) {
		verts[i].x = allInside ? 0.0f : -999.0f;
		verts[i].y = allInside ? 0.0f : -999.0f;
		verts[i].z = allInside ? 0.0f : -999.0f;
	}
	if (iSwap >= 0) {
		for (int i = iSwap; i < iSwap + swapc; ++i) {
			verts[i].x = !allInside ? 0.0f : -999.0f;
			verts[i].y = !allInside ? 0.0f : -999.0f;
			verts[i].z = !allInside ? 0.0f : -999.0f;
		}
	}

	sceGuSendCommandi(GE_CMD_AMBIENTALPHA, 1);
	sceGuSendCommandi(GE_CMD_VERTEXTYPE, GU_VERTEX_32BITF | GU_TRANSFORM_3D);
	sceGuSendCommandi(GE_CMD_BASE, (((uintptr_t)verts) >> 8) & 0x00FF0000);
	sceGuSendCommandi(GE_CMD_VADDR, ((uintptr_t)verts) & 0x00FFFFFF);
	sceGuSendCommandi(GE_CMD_BOUNDINGBOX, c);
	sceGuSendCommandi(GE_CMD_BJUMP, ((uintptr_t)gu_list->current + 8) & 0x00FFFFFF);
	sceGuSendCommandi(GE_CMD_AMBIENTALPHA, 2);
	sceGuSendCommandi(GE_CMD_NOP, 0);

	endFrame();

	checkpoint("%s: %08x", title, sceGeGetCmd(GE_CMD_AMBIENTALPHA));
}

extern "C" int main(int argc, char *argv[]) {
	initDisplay();
	scePowerSetClockFrequency(333, 333, 166);

	checkpointNext("Basic:");
	testVerts("  Single inside", 1, true);
	testVerts("  Single outside", 1, false);

	checkpointNext("Counts:");
	testVerts("  Zero", 0, true, true);
	testVerts("  0x100 inside", 0x100, true);
	testVerts("  0x100 outside", 0x100, false);
	testVerts("  0x1000 inside", 0x1000, true);
	testVerts("  0x1000 outside", 0x1000, false);
	testVerts("  0xFFFF inside", 0xFFFF, true);
	testVerts("  0xFFFF outside", 0xFFFF, false);
	testVerts("  0x10000 inside", 0x10000, true);
	testVerts("  0x10000 outside", 0x10000, false);
	testVerts("  0x10100 inside", 0x10100, true);
	testVerts("  0x10100 outside", 0x10100, false);

	checkpointNext("Values:");
	testVerts("  0xFF outside, 0xFF00 inside", 0xFFFF, false, 0x00FF, 0xFF00);
	testVerts("  0x100 outside, 0xFEFF inside", 0xFFFF, false, 0x0100, 0xFEFF);
	testVerts("  0x101 outside, 0xFEFE inside", 0xFFFF, false, 0x0101, 0xFEFE);
	testVerts("  0xE00 outside, 0x100 inside, 0x100 outside", 0x1000, false, 0x0E00, 0x100);
	testVerts("  0xE00 outside, 1 inside, 0x1FF outside", 0x1000, false, 0x0E00, 1);
	testVerts("  0xEFF outside, 1 inside, 0x100 outside", 0x1000, false, 0x0EFF, 1);
	testVerts("  0xF00 outside, 1 inside, 0xFF outside", 0x1000, false, 0x0F00, 1);
	testVerts("  0x1E00 outside, 0x100 inside, 0x100 outside", 0x2000, false, 0x1E00, 0x100);
	testVerts("  0x1DFF outside, 1 inside, 0x200 outside", 0x2000, false, 0x1DFF, 1);
	testVerts("  0x1E00 outside, 1 inside, 0x1FF outside", 0x2000, false, 0x1E00, 1);
	testVerts("  0x1EFF outside, 1 inside, 0x100 outside", 0x2000, false, 0x1EFF, 1);
	testVerts("  0x1F00 outside, 1 inside, 0xFF outside", 0x2000, false, 0x1F00, 1);
	testVerts("  0x100 inside, last outside", 0x100, true, 0xFF);
	testVerts("  0x100 outside, last inside", 0x100, false, 0xFF);
	testVerts("  0x101 inside, last outside", 0x101, true, 0x100);
	testVerts("  0x101 outside, last inside", 0x101, false, 0x100);
	testVerts("  1 inside, 0x100 outside", 0x101, true, 1, 0x100);
	testVerts("  1 outside, 0x100 inside", 0x101, false, 1, 0x100);
	testVerts("  0xFFFF inside, last outside", 0xFFFF, true, 0xFFFE);
	testVerts("  0xFFFF outside, last inside", 0xFFFF, false, 0xFFFE);
	testVerts("  0xFDFE outside, 1 inside, 0x200 outside", 0xFFFF, true, 0xFDFE, 1);
	testVerts("  0xFDFF outside, 1 inside, 0x1FF outside", 0xFFFF, true, 0xFDFF, 1);

	sceGuTerm();
	return 0;
}
