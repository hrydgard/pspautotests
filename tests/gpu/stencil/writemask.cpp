#include <common.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>

// The alpha/stencil write mask (GE command 0xE9) on stencil ops, with the stencil test off, and on
// clears, in the three formats that have stencil bits. Each case first sets every pixel's stencil to
// INIT, then draws once with the mask and reads back the first pixel.

extern "C" int sceDmacMemcpy(void *dest, const void *source, unsigned int size);

typedef struct {
	u32 color;
	float x, y, z;
} VertexColorF32;

static u8 *fbp0 = 0;
static u8 *dbp0 = fbp0 + 512 * 272 * sizeof(u32);

static u32 __attribute__((aligned(64))) copybuf[16];
static unsigned int __attribute__((aligned(16))) list[262144];
static __attribute__((aligned(16))) VertexColorF32 vertices[2];

enum Kind {
	KIND_STENCIL,
	KIND_STENCIL_OFF,
	KIND_CLEAR,
};

struct Op {
	const char *name;
	Kind kind;
	int op;
	int ref;
};

static const Op ops[] = {
	{ "REPLACE C3", KIND_STENCIL, GU_REPLACE, 0xC3 },
	{ "INVERT", KIND_STENCIL, GU_INVERT, 0 },
	{ "INCR", KIND_STENCIL, GU_INCR, 0 },
	{ "DECR", KIND_STENCIL, GU_DECR, 0 },
	{ "ZERO", KIND_STENCIL, GU_ZERO, 0 },
	{ "stencil off", KIND_STENCIL_OFF, 0, 0 },
	{ "clear 3C", KIND_CLEAR, 0, 0 },
};

static const u8 masks[] = { 0x00, 0xFF, 0x0F, 0xF0, 0x7F, 0x80, 0x55, 0xFE };
static const u8 inits[] = { 0x00, 0xFF, 0x5A };

static const u32 COLOR = 0x3C406080;

static void drawSprite(u32 c) {
	vertices[0].color = c;
	vertices[0].x = 0.0f;
	vertices[0].y = 0.0f;
	vertices[0].z = 0.0f;
	vertices[1].color = c;
	vertices[1].x = 16.0f;
	vertices[1].y = 16.0f;
	vertices[1].z = 0.0f;
	sceKernelDcacheWritebackInvalidateAll();
	sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, NULL, vertices);
}

static u32 runCase(int psm, u8 init, const Op &op, u8 mask) {
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(psm, fbp0, 512);

	// Every pixel's stencil to init, color to 0.
	sceGuSendCommandi(0xE8, 0);
	sceGuSendCommandi(0xE9, 0);
	sceGuEnable(GU_STENCIL_TEST);
	sceGuStencilFunc(GU_ALWAYS, init, 0xFF);
	sceGuStencilOp(GU_REPLACE, GU_REPLACE, GU_REPLACE);
	drawSprite(0);

	sceGuSendCommandi(0xE9, mask);
	switch (op.kind) {
	case KIND_STENCIL:
		sceGuStencilFunc(GU_ALWAYS, op.ref, 0xFF);
		sceGuStencilOp(op.op, op.op, op.op);
		drawSprite(COLOR);
		break;
	case KIND_STENCIL_OFF:
		sceGuDisable(GU_STENCIL_TEST);
		drawSprite(COLOR);
		break;
	case KIND_CLEAR:
		sceGuDisable(GU_STENCIL_TEST);
		sceGuClearColor(COLOR & 0x00FFFFFF);
		sceGuClearStencil(COLOR >> 24);
		sceGuClear(GU_COLOR_BUFFER_BIT | GU_STENCIL_BUFFER_BIT);
		break;
	}

	sceGuSendCommandi(0xE9, 0);
	sceGuDisable(GU_STENCIL_TEST);
	sceGuFinish();
	sceGuSync(GU_SYNC_WAIT, GU_SYNC_WHAT_DONE);

	sceKernelDcacheWritebackInvalidateAll();
	sceDmacMemcpy(copybuf, sceGeEdramGetAddr(), sizeof(copybuf));
	sceKernelDcacheWritebackInvalidateAll();
	if (psm == GU_PSM_8888) {
		return copybuf[0];
	}
	return copybuf[0] & 0xFFFF;
}

static void init() {
	sceGuInit();
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888, fbp0, 512);
	sceGuDispBuffer(480, 272, fbp0, 512);
	sceGuDepthBuffer(dbp0, 512);
	sceGuOffset(2048 - (240 / 2), 2048 - (136 / 2));
	sceGuViewport(2048, 2048, 240, 136);
	sceGuDepthRange(65535, 0);
	sceGuDepthMask(1);
	sceGuDisable(GU_DEPTH_TEST);
	sceGuScissor(0, 0, 480, 272);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuDisable(GU_TEXTURE_2D);
	sceGuDisable(GU_BLEND);
	sceGuDisable(GU_ALPHA_TEST);
	sceGuDisable(GU_DITHER);
	sceGuFinish();
	sceGuSync(0, 0);

	sceDisplayWaitVblankStart();
	sceGuDisplay(1);
}

extern "C" int main(int argc, char *argv[]) {
	init();

	struct Format {
		int psm;
		const char *name;
	};
	static const Format formats[] = {
		{ GU_PSM_8888, "8888" },
		{ GU_PSM_4444, "4444" },
		{ GU_PSM_5551, "5551" },
	};

	for (const Format &f : formats) {
		for (u8 init : inits) {
			schedf("%s, stencil %02x:\n", f.name, init);
			for (const Op &op : ops) {
				char line[256];
				int len = snprintf(line, sizeof(line), "  %-12s", op.name);
				for (u8 mask : masks) {
					u32 v = runCase(f.psm, init, op, mask);
					len += snprintf(line + len, sizeof(line) - len, f.psm == GU_PSM_8888 ? " %02x:%08x" : " %02x:%04x", mask, (unsigned)v);
				}
				schedf("%s\n", line);
			}
		}
	}

	sceGuTerm();
	return 0;
}
