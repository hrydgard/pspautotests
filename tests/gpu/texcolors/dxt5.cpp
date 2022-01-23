#include <common.h>
#include <malloc.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>

extern "C" int sceDmacMemcpy(void *dest, const void *source, unsigned int size);
extern "C" void sendCommandi(int cmd, int argument);

extern int HAS_DISPLAY;

// Through mode or transform?
const bool TEST_TRANSFORM = false;

u8 *fbp0 = 0;
u8 *dbp0 = fbp0 + 512 * 272 * sizeof(u32);

static u32 copybuf[512 * 272];
u32 *drawbuf;

unsigned int __attribute__((aligned(16))) list[262144];

enum {
	IMG_WHITE_0,
	IMG_WHITE_1,
	IMG_WHITE_2,
	IMG_WHITE_3,
	IMG_GT_0,
	IMG_GT_1,
	IMG_GT_2,
	IMG_GT_3,
	IMG_LT_0,
	IMG_LT_1,
	IMG_LT_2,
	IMG_LT_3,
	IMG_MIX_2,
	IMG_MIX_3,
	IMG_ALPHA_GT,
	IMG_ALPHA_EQ,
	IMG_TL_ONLY,

	IMG_COUNT,
};

struct DXT1Block {
	u8 lines[4];
	u16 color1;
	u16 color2;
};

struct DXT5Block {
	DXT1Block color;
	u32 alphadata2;
	u16 alphadata1;
	u8 alpha1; u8 alpha2;
};

DXT5Block *imageData[IMG_COUNT] = {};

typedef struct {
	u16 u, v;
	s16 x, y, z;
} Vertex;

inline s16 norm16x(int x) { return x * 65536 / 480 - 32768; }
inline s16 norm16y(int x) { return 32767 - x * 65536 / 272; }

Vertex verticesThrough[2] = { {0, 0, 0, 0, 0}, {2, 2, 2, 2, 0} };
Vertex verticesTransform[2] = { {0, 0, norm16x(0), norm16y(0), 65500}, {256, 256, norm16x(2), norm16y(2), 65535} };

int getBuffer() {
	sceKernelDcacheWritebackInvalidateRange(copybuf, sizeof(copybuf));
	sceKernelDcacheWritebackInvalidateRange(drawbuf, sizeof(copybuf));
	sceDmacMemcpy(copybuf, drawbuf, 512 * 4);
	sceKernelDcacheWritebackInvalidateRange(copybuf, sizeof(copybuf));
	sceKernelDcacheWritebackInvalidateRange(drawbuf, sizeof(copybuf));

	return copybuf[0] & 0x00FFFFFF;
}

void resetBuffer(int c) {
	memset(copybuf, c, 512 * 4);
	sceKernelDcacheWritebackInvalidateRange(copybuf, sizeof(copybuf));
	sceKernelDcacheWritebackInvalidateRange(drawbuf, sizeof(copybuf));
	sceDmacMemcpy(drawbuf, copybuf, 512 * 4);
	sceKernelDcacheWritebackInvalidateRange(copybuf, sizeof(copybuf));
	sceKernelDcacheWritebackInvalidateRange(drawbuf, sizeof(copybuf));
}

DXT5Block *getTexturePtr(int img, int alpha) {
	return imageData[img] + alpha * 4;
}

void drawTexFlush(int img, int alpha, const void *verts, bool exposeAlpha) {
	sceGuStart(GU_DIRECT, list);

	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexSync();
	sceGuTexFlush();
	sceGuTexMode(GU_PSM_DXT5, 0, 0, GU_FALSE);
	sceGuTexWrap(GU_CLAMP, GU_CLAMP);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	sceGuTexSlope(2.0f);
	sceGuTexImage(0, 8, 8, 8, getTexturePtr(img, alpha));

	if (exposeAlpha) {
		sceGuEnable(GU_BLEND);
		sceGuBlendFunc(GU_ADD, GU_FIX, GU_SRC_ALPHA, 0, 0xFFFFFF);
	} else {
		sceGuDisable(GU_BLEND);
	}

	if (TEST_TRANSFORM) {
		sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_3D, 2, NULL, verts);
	} else {
		sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, verts);
	}

	sceGuFinish();
	sceGuSync(0, 0);
	sceGuSync(GU_SYNC_DONE, GU_SYNC_WHAT_DRAW);
	sceKernelDelayThread(200);
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
	sceGuScissor(0, 0, 8, 8);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuDisable(GU_DITHER);
	sceGuFrontFace(GU_CW);
	sceGuShadeModel(GU_SMOOTH);

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

	resetBuffer(0);

	// To avoid text writes.
	HAS_DISPLAY = 0;
}

void fillTexture(int img, u16 c1, u16 c2, u8 line, u8 a1, u8 a2) {
	if (imageData[img] == NULL) {
		imageData[img] = (DXT5Block *)memalign(16, sizeof(DXT5Block) * 4 * 8);
	}

	DXT5Block *p = imageData[img];
	for (int alpha = 0; alpha < 8; ++alpha) {
		u32 alpha12 = (alpha << 0) | (alpha << 3) | (alpha << 6) | (alpha << 9);
		u32 alpha32 = (alpha12 << 0) | (alpha12 << 12) | (alpha12 << 24);

		for (int i = 0; i < 4; ++i) {
			p[alpha * 4 + i].color.color1 = c1;
			p[alpha * 4 + i].color.color2 = c2;
			for (int y = 0; y < 4; ++y) {
				p[alpha * 4 + i].color.lines[y] = line;
				p[alpha * 4 + i].alpha1 = a1;
				p[alpha * 4 + i].alpha2 = a2;
				p[alpha * 4 + i].alphadata1 = (u16)alpha32;
				p[alpha * 4 + i].alphadata2 = alpha32;
			}
		}
	}
}

void setupTexture() {
	fillTexture(IMG_WHITE_0, 0xFFFF, 0xFFFF, 0, 0x55, 0xFF);
	fillTexture(IMG_WHITE_1, 0xFFFF, 0xFFFF, 0x55, 0x55, 0xFF);
	fillTexture(IMG_WHITE_2, 0xFFFF, 0xFFFF, 0xAA, 0x55, 0xFF);
	fillTexture(IMG_WHITE_3, 0xFFFF, 0xFFFF, 0xFF, 0x55, 0xFF);

	fillTexture(IMG_GT_0, 0xFFE3, 0x8410, 0, 0x55, 0xFF);
	fillTexture(IMG_GT_1, 0xFFE3, 0x8410, 0x55, 0x55, 0xFF);
	fillTexture(IMG_GT_2, 0xFFE3, 0x8410, 0xAA, 0x55, 0xFF);
	fillTexture(IMG_GT_3, 0xFFE3, 0x8410, 0xFF, 0x55, 0xFF);

	fillTexture(IMG_LT_0, 0x8410, 0xF85F, 0, 0x55, 0xFF);
	fillTexture(IMG_LT_1, 0x8410, 0xF85F, 0x55, 0x55, 0xFF);
	fillTexture(IMG_LT_2, 0x8410, 0xF85F, 0xAA, 0x55, 0xFF);
	fillTexture(IMG_LT_3, 0x8410, 0xF85F, 0xFF, 0x55, 0xFF);

	fillTexture(IMG_MIX_2, 0x7890, 0x1234, 0xAA, 0x55, 0xFF);
	fillTexture(IMG_MIX_3, 0x7890, 0x1234, 0xFF, 0x55, 0xFF);

	//fillTexture(IMG_ALPHA_GT, 0x7890, 0x1234, 0xAA, 0xE7, 0x34);
	fillTexture(IMG_ALPHA_GT, 0x7890, 0x1234, 0xAA, 0xFF, 0x00);
	//fillTexture(IMG_ALPHA_EQ, 0x7890, 0x1234, 0xFF, 0xFD, 0xFD);
	fillTexture(IMG_ALPHA_EQ, 0x7890, 0x1234, 0xAA, 0x00, 0xFF);

	imageData[IMG_TL_ONLY] = (DXT5Block *)memalign(16, sizeof(DXT5Block) * 4 * 8);

	DXT5Block *p = imageData[IMG_TL_ONLY];
	for (int alpha = 0; alpha < 8; ++alpha) {
		for (int i = 0; i < 4; ++i) {
			p[alpha * 4 + i].color.color1 = 0x7777;
			p[alpha * 4 + i].color.color2 = 0x1356;
			for (int y = 0; y < 4; ++y) {
				p[alpha * 4 + i].color.lines[y] = y == 0 ? 0x02 : 0x00;
				p[alpha * 4 + i].alpha1 = 0xE1;
				p[alpha * 4 + i].alpha2 = 0x59;
				p[alpha * 4 + i].alphadata1 = 0;
				p[alpha * 4 + i].alphadata2 = alpha;
			}
		}
	}

	sceKernelDcacheWritebackInvalidateRange(p, sizeof(DXT5Block) * 4 * 8);
}

void testDrawingBox(const char *title, int img) {
	const void *verts = TEST_TRANSFORM ? verticesTransform : verticesThrough;

	for (int alpha = 0; alpha < 8; ++alpha) {
		drawTexFlush(img, alpha, verts, false);
		int color = getBuffer();
		resetBuffer(0xFF);

		drawTexFlush(img, alpha, verts, true);
		int resultAlpha = getBuffer();
		resetBuffer(0x00);

		checkpoint("%s (alpha=%d): %06x / %06x", title, alpha, color, resultAlpha);
	}
}

extern "C" int main(int argc, char *argv[]) {
	init();
	setupTexture();

	sceDisplaySetFrameBuf(sceGeEdramGetAddr(), 512, GU_PSM_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
	sceDisplaySetMode(0, 480, 272);

	sceGuStart(GU_DIRECT, list);
	sceGuTexFilter(GU_NEAREST, GU_NEAREST);
	sceGuFinish();
	sceGuSync(0, 0);

	testDrawingBox("White 0", IMG_WHITE_0);
	testDrawingBox("White 1", IMG_WHITE_1);
	testDrawingBox("White 2", IMG_WHITE_2);
	testDrawingBox("White 3", IMG_WHITE_3);

	testDrawingBox("First greater 0", IMG_GT_0);
	testDrawingBox("First greater 1", IMG_GT_1);
	testDrawingBox("First greater 2", IMG_GT_2);
	testDrawingBox("First greater 3", IMG_GT_3);

	testDrawingBox("First lesser 0", IMG_LT_0);
	testDrawingBox("First lesser 1", IMG_LT_1);
	testDrawingBox("First lesser 2", IMG_LT_2);
	testDrawingBox("First lesser 3", IMG_LT_3);

	testDrawingBox("Color mix 2", IMG_MIX_2);
	testDrawingBox("Color mix 3", IMG_MIX_3);

	testDrawingBox("Alpha greater", IMG_ALPHA_GT);
	testDrawingBox("Alpha equal", IMG_ALPHA_EQ);

	testDrawingBox("Top left pixel only", IMG_TL_ONLY);

	for (int a2 = 0; a2 < 0x100; ++a2) {
		fillTexture(IMG_ALPHA_GT, 0x7890, 0x1234, 0xAA, 0xFF, a2);
		char temp[128];
		sprintf(temp, "Alpha %02x", a2);
		testDrawingBox(temp, IMG_ALPHA_GT);

		if ((a2 & 0x0F) == 0) {
			flushschedf();
		}
	}

	sceGuTerm();

	for (int i = 0; i < IMG_COUNT; ++i) {
		free(imageData[i]);
	}

	return 0;
}
