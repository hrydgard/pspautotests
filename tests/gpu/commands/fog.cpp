#include <common.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>

extern "C" int sceDmacMemcpy(void *dest, const void *source, unsigned int size);

#define GU_IMM_FOG 0x00400000

typedef struct {
	u32 color;
	float x, y, z;
} VertexColorF32;

static u8 *fbp0 = 0;
static u8 *dbp0 = fbp0 + 512 * 272 * sizeof(u32);

static u32 copybuf[32 * 1];
static unsigned int __attribute__((aligned(16))) list[131072];
static __attribute__((aligned(16))) VertexColorF32 vertices_f32[256];

inline VertexColorF32 makeVertex32(u32 c, float x, float y, float z) {
	VertexColorF32 v;
	v.color = c;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

void displayBuffer(const char *reason) {
	sceKernelDcacheWritebackInvalidateRange(copybuf, sizeof(copybuf));
	sceDmacMemcpy(copybuf, sceGeEdramGetAddr(), sizeof(copybuf));
	const u32 *buf = copybuf;

	checkpoint("%s: COLOR=%08x", reason, buf[0]);

	// Reset.
	memset(copybuf, 0x44, sizeof(copybuf));
	sceKernelDcacheWritebackInvalidateRange(copybuf, sizeof(copybuf));
	sceDmacMemcpy(sceGeEdramGetAddr(), copybuf, sizeof(copybuf));
}

void drawBoxCommands(u32 c) {
	vertices_f32[0] = makeVertex32(c, -1.0, -1.0, 0.0);
	vertices_f32[1] = makeVertex32(c, 1.0, 1.0, 0.0);
	sceKernelDcacheWritebackInvalidateRange(vertices_f32, sizeof(vertices_f32));

	sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 2, NULL, vertices_f32);
}

void drawBoxCommandsImm(u32 c, u8 fogfactor) {
	sceGuSendCommandi(18, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D);

	sceGuSendCommandi(0xF0, (2048 - (480 / 2)) << 4);
	sceGuSendCommandi(0xF1, (2048 - (272 / 2)) << 4);
	sceGuSendCommandi(0xF2, 0);
	sceGuSendCommandi(0xF6, c & 0x00FFFFFF);
	sceGuSendCommandi(0xF9, 0);
	sceGuSendCommandi(0xF8, fogfactor);
	sceGuSendCommandi(0xF7, GU_IMM_FOG | (GU_SPRITES << 8) | (c >> 24));

	sceGuSendCommandi(0xF0, (2048 + (480 / 2)) << 4);
	sceGuSendCommandi(0xF1, (2048 + (272 / 2)) << 4);
	sceGuSendCommandi(0xF7, GU_IMM_FOG | (7 << 8) | (c >> 24));
}

void init() { 
	sceGuInit();
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888, fbp0, 512);
	sceGuDispBuffer(480, 272, fbp0, 512);
	sceGuDepthBuffer(dbp0, 512);
	sceGuOffset(2048 - (480 / 2), 2048 - (272 / 2));
	sceGuViewport(2048, 2048, 480, 272);
	sceGuDepthRange(65535, 0);
	sceGuDepthMask(0);
	sceGuScissor(0, 0, 10, 10);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFrontFace(GU_CW);
	sceGuShadeModel(GU_SMOOTH);
	sceGuDisable(GU_TEXTURE_2D);

	ScePspFMatrix4 ones = {
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{0, 0, 0, 1},
	};

	sceGuSetMatrix(GU_MODEL, &ones);
	sceGuSetMatrix(GU_VIEW, &ones);
	sceGuSetMatrix(GU_PROJECTION, &ones);

	sceGuFinish();
	sceGuSync(0, 0);
 
	sceDisplayWaitVblankStart();
	sceGuDisplay(1);

	memset(copybuf, 0x44, sizeof(copybuf));
	sceKernelDcacheWritebackInvalidateRange(copybuf, sizeof(copybuf));
	sceDmacMemcpy(sceGeEdramGetAddr(), copybuf, sizeof(copybuf));

	displayBuffer("Initial");
}

void testFog(const char *title, u32 prev, u32 c, u32 fogc, float fog_near, float fog_far) {
	for (size_t i = 0; i < sizeof(copybuf) / 4; ++i) {
		copybuf[i] = prev;
	}
	sceKernelDcacheWritebackInvalidateRange(copybuf, sizeof(copybuf));
	sceDmacMemcpy(sceGeEdramGetAddr(), copybuf, sizeof(copybuf));

	sceGuStart(GU_DIRECT, list);

	sceGuDisable(GU_BLEND);
	sceGuDisable(GU_STENCIL_TEST);
	sceGuEnable(GU_FOG);

	sceGuFog(fog_near, fog_far, fogc);

	drawBoxCommands(c);

	sceGuFinish();
	sceGuSync(GU_SYNC_WAIT, GU_SYNC_WHAT_DONE);

	displayBuffer(title);
}

void testImmFog(const char *title, u32 prev, u32 c, u32 fogc, u8 fogfactor) {
	for (size_t i = 0; i < sizeof(copybuf) / 4; ++i) {
		copybuf[i] = prev;
	}
	sceKernelDcacheWritebackInvalidateRange(copybuf, sizeof(copybuf));
	sceDmacMemcpy(sceGeEdramGetAddr(), copybuf, sizeof(copybuf));

	sceGuStart(GU_DIRECT, list);

	sceGuDisable(GU_BLEND);
	sceGuDisable(GU_STENCIL_TEST);
	sceGuEnable(GU_FOG);

	sceGuFog(0.0f, 1.0f, fogc);

	drawBoxCommandsImm(c, fogfactor);

	sceGuFinish();
	sceGuSync(GU_SYNC_WAIT, GU_SYNC_WHAT_DONE);

	displayBuffer(title);
}

extern volatile int didResched;

extern "C" int main(int argc, char *argv[]) {
	init();

	schedf("framebuf: %08x\n", sceDisplaySetFrameBuf(sceGeEdramGetAddr(), 512, GU_PSM_8888, PSP_DISPLAY_SETBUF_IMMEDIATE));
	schedf("dispmode: %08x\n", sceDisplaySetMode(0, 480, 272));

	checkpointNext("Common:");
	testFog("  Basic", 0x44444444, 0x88888888, 0xFFFFFFFF, 1.0f, 1.0f);
	testFog("  Near neg", 0x44444444, 0x88888888, 0xFFFFFFFF, -1.0f, 1.0f);
	testFog("  Far neg", 0x44444444, 0x88888888, 0xFFFFFFFF, 1.0f, -1.0f);
	testFog("  Both neg", 0x44444444, 0x88888888, 0xFFFFFFFF, -1.0f, -1.0f);
	testFog("  Near zero", 0x44444444, 0x88888888, 0xFFFFFFFF, 0.0f, 1.0f);
	testFog("  Far zero", 0x44444444, 0x88888888, 0xFFFFFFFF, 1.0f, 0.0f);
	testImmFog("  Direct fog 00", 0x44444444, 0x88888888, 0xFFFFFFFF, 0x00);
	testImmFog("  Direct fog 80", 0x44444444, 0x88888888, 0xFFFFFFFF, 0x80);
	testImmFog("  Direct fog FF", 0x44444444, 0x88888888, 0xFFFFFFFF, 0xFF);

	checkpointNext("Rounding:");
	for (int i = 0; i < 256; ++i) {
		char temp[128];
		sprintf(temp, "  Fog value %02x", i);
		testImmFog(temp, 0x44444444, 0x11881100, 0xFFFF33FF, i);
	}
	// More exhaustive test.
	/*for (int i = 0; i < 256; ++i) {
		char temp[128];
		snprintf(temp, 128, "  0x7F * 0x%02X - Zero (%f / %d)", i, i * 127.0 / 255.0, (i * 127) % 255);

		for (int j = 0; j < 256; ++j) {
			int jjj = j | (j << 8) | (j << 16);
			snprintf(temp, 128, "%d,%d", i, j);
			testImmFog(temp, 0x00000000, jjj, 0x00FFFFFF, (u8)i);
		}
		if (i != 255) {
			flushschedf();
			didResched = 0;
		}
	}*/

	sceGuTerm();

	return 0;
}
