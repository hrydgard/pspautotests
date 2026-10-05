#include <common.h>
#include <malloc.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>

extern "C" int sceDmacMemcpy(void *dest, const void *source, unsigned int size);

extern int HAS_DISPLAY;

// A mipmapped texture whose second level has an invalid address (0), drawn at a constant level 0, so that level
// is never sampled. Tactics Ogre's font is drawn like this. Valid second levels for comparison.

u8 *fbp0 = 0;
u8 *dbp0 = fbp0 + 512 * 272 * sizeof(u32);

static u32 copybuf[512 * 272];
u32 *drawbuf;

unsigned int __attribute__((aligned(16))) list[262144];

static u32 __attribute__((aligned(16))) tex8888[2][16 * 16];
// CLUT4 rows are 32 texels apart, the smallest buffer width (16 bytes).
static u8 __attribute__((aligned(16))) texClut4[2][32 * 16 / 2];
static u32 __attribute__((aligned(16))) clut[16];

typedef struct {
	u16 u, v;
	s16 x, y, z;
} Vertex;

static Vertex vertices[2] = {
	{ 0, 0, 0, 0, 0 },
	{ 16, 16, 16, 16, 0 },
};

void displayBuffer(const char *reason) {
	sceKernelDcacheWritebackInvalidateAll();
	sceDmacMemcpy(copybuf, drawbuf, sizeof(copybuf));
	sceKernelDcacheWritebackInvalidateAll();
	const u32 *buf = copybuf;

	checkpoint("%s: %08x %08x %08x", reason, buf[0], buf[3 * 512 + 5], buf[15 * 512 + 15]);

	// Reset.
	memset(copybuf, 0, sizeof(copybuf));
	sceKernelDcacheWritebackInvalidateAll();
	sceDmacMemcpy(drawbuf, copybuf, sizeof(copybuf));
	sceKernelDcacheWritebackInvalidateAll();
}

void draw(bool clut4, bool linear, bool validLevel1) {
	sceGuStart(GU_DIRECT, list);

	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexSync();
	sceGuTexFlush();
	// Texture level mode const, bias 0: always level 0.
	sceGuSendCommandi(200, GU_TEXTURE_CONST);
	if (linear) {
		sceGuTexFilter(GU_LINEAR_MIPMAP_LINEAR, GU_LINEAR);
	} else {
		sceGuTexFilter(GU_NEAREST_MIPMAP_LINEAR, GU_NEAREST);
	}
	sceGuTexWrap(GU_CLAMP, GU_CLAMP);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	if (clut4) {
		sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
		sceGuClutLoad(16 / 8, clut);
		sceGuTexMode(GU_PSM_T4, 1, 0, GU_FALSE);
		sceGuTexImage(0, 16, 16, 32, texClut4[0]);
		sceGuTexImage(1, 8, 8, 32, validLevel1 ? texClut4[1] : NULL);
	} else {
		sceGuTexMode(GU_PSM_8888, 1, 0, GU_FALSE);
		sceGuTexImage(0, 16, 16, 16, tex8888[0]);
		sceGuTexImage(1, 8, 8, 8, validLevel1 ? tex8888[1] : NULL);
	}

	sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, vertices);

	sceGuFinish();
	sceGuSync(0, 0);
}

void testDraw(const char *title, bool clut4, bool linear, bool validLevel1) {
	draw(clut4, linear, validLevel1);
	sceDisplayWaitVblankStart();
	displayBuffer(title);
}

void init() {
	void *fbp0 = 0;

	drawbuf = (u32 *)sceGeEdramGetAddr();

	sceGuInit();
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888, fbp0, 512);
	sceGuDispBuffer(480, 272, fbp0, 512);
	sceGuDepthBuffer(dbp0, 512);
	sceGuOffset(2048 - (480 / 2), 2048 - (272 / 2));
	sceGuViewport(2048, 2048, 480, 272);
	sceGuDepthRange(65535, 65500);
	sceGuDepthMask(0);
	sceGuScissor(0, 0, 480, 272);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFrontFace(GU_CW);
	sceGuShadeModel(GU_SMOOTH);
	sceGuFinish();
	sceGuSync(0, 0);

	sceDisplayWaitVblankStart();
	sceGuDisplay(1);

	memset(copybuf, 0, sizeof(copybuf));
	sceKernelDcacheWritebackInvalidateAll();
	sceDmacMemcpy(drawbuf, copybuf, sizeof(copybuf));
	sceKernelDcacheWritebackInvalidateAll();

	// To avoid text writes.
	HAS_DISPLAY = 0;
}

void setupTextures() {
	// All different and none black, the framebuffer's color before each draw.
	for (int i = 0; i < 16; ++i) {
		clut[i] = 0xFF000000 | ((0x0F + i * 0x10) << 16) | ((0xF0 - i * 0x08) << 8) | (0x20 + i * 0x04);
	}
	for (int level = 0; level < 2; ++level) {
		for (int y = 0; y < 16; ++y) {
			for (int x = 0; x < 16; ++x) {
				// Level 1 is all one color, so a blend with it would show.
				const int index = level == 0 ? (x + y) & 15 : 15;
				tex8888[level][y * 16 + x] = clut[index];
				u8 &b = texClut4[level][(y * 32 + x) / 2];
				if (x & 1)
					b = (b & 0x0F) | (index << 4);
				else
					b = (b & 0xF0) | index;
			}
		}
	}
	sceKernelDcacheWritebackInvalidateAll();
}

extern "C" int main(int argc, char *argv[]) {
	init();
	setupTextures();

	sceDisplaySetFrameBuf(sceGeEdramGetAddr(), 512, GU_PSM_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
	sceDisplaySetMode(0, 480, 272);

	checkpointNext("Level 1 valid:");
	testDraw("  8888 nearest", false, false, true);
	testDraw("  8888 linear", false, true, true);
	testDraw("  CLUT4 nearest", true, false, true);
	testDraw("  CLUT4 linear", true, true, true);

	checkpointNext("Level 1 invalid:");
	testDraw("  8888 nearest", false, false, false);
	testDraw("  8888 linear", false, true, false);
	testDraw("  CLUT4 nearest", true, false, false);
	testDraw("  CLUT4 linear", true, true, false);

	sceGuTerm();
	return 0;
}
