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
static __attribute__((aligned(16))) VertexF32 vertices_f32[0x300];
static __attribute__((aligned(16))) uint16_t indices_16[0x300];

static void testVerts(const char *title, int c, const VertexF32 *verts, bool useIndices, const uint16_t *inds, int vtype = -1) {
	PspGeContext before, after;

	sceGeSaveContext(&before);
	startFrame();

	sceGuSendCommandi(GE_CMD_AMBIENTALPHA, 1);
	if (vtype != -1) {
		sceGuSendCommandi(GE_CMD_VERTEXTYPE, vtype);
	} else if (useIndices) {
		sceGuSendCommandi(GE_CMD_VERTEXTYPE, GU_VERTEX_32BITF | GU_INDEX_16BIT | GU_TRANSFORM_3D);
	} else {
		sceGuSendCommandi(GE_CMD_VERTEXTYPE, GU_VERTEX_32BITF | GU_TRANSFORM_3D);
	}
	sceGuSendCommandi(GE_CMD_BASE, (((uintptr_t)verts) >> 8) & 0x00FF0000);
	sceGuSendCommandi(GE_CMD_VADDR, ((uintptr_t)verts) & 0x00FFFFFF);
	sceGuSendCommandi(GE_CMD_BASE, (((uintptr_t)inds) >> 8) & 0x00FF0000);
	sceGuSendCommandi(GE_CMD_IADDR, ((uintptr_t)inds) & 0x00FFFFFF);
	sceGuSendCommandi(GE_CMD_BOUNDINGBOX, c);
	sceGuSendCommandi(GE_CMD_BJUMP, ((uintptr_t)gu_list->current + 8) & 0x00FFFFFF);
	sceGuSendCommandi(GE_CMD_AMBIENTALPHA, 2);
	sceGuSendCommandi(GE_CMD_NOP, 0);

	endFrame();
	sceGeSaveContext(&after);

	ptrdiff_t vchange = after.context[5] - (uintptr_t)verts;
	ptrdiff_t ichange = after.context[6] - (uintptr_t)inds;
	checkpoint("%s: %08x (v +%d, i +%d)", title, sceGeGetCmd(GE_CMD_AMBIENTALPHA), vchange, ichange);
}

extern "C" int main(int argc, char *argv[]) {
	initDisplay();
	scePowerSetClockFrequency(333, 333, 166);

	checkpointNext("Advancement:");
	testVerts("  Positions F32 advance", 1, vertices_f32, false, NULL);
	testVerts("  Positions S16 advance", 1, vertices_f32, false, NULL, GU_VERTEX_16BIT | GU_TRANSFORM_3D);
	testVerts("  Positions S8 advance", 1, vertices_f32, false, NULL, GU_VERTEX_8BIT | GU_TRANSFORM_3D);
	testVerts("  Throughmode advance", 1, vertices_f32, false, NULL, GU_VERTEX_32BITF);
	testVerts("  Positions/Normal/TC S8 advance", 1, vertices_f32, false, NULL, GU_VERTEX_8BIT | GU_NORMAL_8BIT | GU_TEXTURE_8BIT | GU_TRANSFORM_3D);
	testVerts("  Large weighted advance", 1, vertices_f32, false, NULL, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_NORMAL_32BITF | GU_VERTEX_32BITF | GU_WEIGHT_32BITF | GU_WEIGHTS(8) | GU_TRANSFORM_3D);
	testVerts("  Large morph advance", 1, vertices_f32, false, NULL, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_NORMAL_32BITF | GU_VERTEX_32BITF | GU_WEIGHT_32BITF | GU_VERTICES(8) | GU_TRANSFORM_3D);
	testVerts("  Large everything advance", 1, vertices_f32, false, NULL, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_NORMAL_32BITF | GU_VERTEX_32BITF | GU_WEIGHT_32BITF | GU_WEIGHTS(8) | GU_VERTICES(8) | GU_TRANSFORM_3D);

	// For indices, let's set everything out of bounds except index 0.
	for (int i = 1; i < 0x300; ++i) {
		vertices_f32[i].x = -999.0f;
		vertices_f32[i].y = -999.0f;
		vertices_f32[i].z = -999.0f;
	}

	// And set the later indices as out.
	for (int i = 0x100; i < 0x200; ++i) {
		indices_16[i] = 1;
	}

	checkpointNext("Indexes:");
	testVerts("  Simple index usage, inside", 64, vertices_f32, true, indices_16);
	testVerts("  8 bit indices, inside", 64, vertices_f32, true, indices_16, GU_INDEX_8BIT | GU_VERTEX_32BITF | GU_TRANSFORM_3D);
	testVerts("  32 bit indices, inside", 64, vertices_f32, true, indices_16, GU_INDEX_BITS | GU_VERTEX_32BITF | GU_TRANSFORM_3D);
	testVerts("  Simple index usage, outside", 64, vertices_f32, true, indices_16 + 0x100);

	// Both cases cause a PSP to hang (possible it trips a signal.)
	//checkpointNext("Errors:");
	//testVerts("  Positions + NULL index", 1, vertices_f32, true, NULL);
	//testVerts("  NULL positions", 1, NULL, false, NULL);

	sceGuTerm();
	return 0;
}
