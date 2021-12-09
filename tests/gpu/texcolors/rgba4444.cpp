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
	IMG_WHITE,
	IMG_RED,
	IMG_GRAY,
	IMG_BROWN,
};

u16 *imageData[4] = {};

typedef struct {
	u16 u, v;
	s16 x, y, z;
} Vertex;

inline s16 norm16x(int x) { return x * 65536 / 480 - 32768; }
inline s16 norm16y(int x) { return 32767 - x * 65536 / 272; }

Vertex verticesThrough[2] = { {0, 0, 0, 0, 0}, {2, 2, 2, 2, 0} };
Vertex verticesTransform[2] = { {0, 0, norm16x(0), norm16y(0), 65500}, {256, 256, norm16x(2), norm16y(2), 65535} };

void displayBuffer(const char *reason) {
	sceKernelDcacheWritebackInvalidateAll();
	sceDmacMemcpy(copybuf, drawbuf, sizeof(copybuf));
	sceKernelDcacheWritebackInvalidateAll();
	const u32 *buf = copybuf;
	int c = buf[0] & 0x00FFFFFF;

	checkpoint("%s: %06x", reason, c);

	// Reset.
	memset(copybuf, 0, sizeof(copybuf));
	sceKernelDcacheWritebackInvalidateAll();
	sceDmacMemcpy(drawbuf, copybuf, sizeof(copybuf));
	sceKernelDcacheWritebackInvalidateAll();
}

u16 *getTexturePtr(int img) {
	return imageData[img];
}

void drawTexFlush(int img, const void *verts) {
	sceGuStart(GU_DIRECT, list);

	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexSync();
	sceGuTexFlush();
	sceGuTexMode(GU_PSM_4444, 0, 0, GU_FALSE);
	sceGuTexWrap(GU_CLAMP, GU_CLAMP);
	sceGuTexFunc(GU_TFX_DECAL, GU_TCC_RGB);
	sceGuTexSlope(2.0f);
	sceGuTexImage(0, 8, 8, 8, getTexturePtr(img));

	if (TEST_TRANSFORM) {
		sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_3D, 2, NULL, verts);
	} else {
		sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, verts);
	}

	sceGuFinish();
	sceGuSync(0, 0);
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

	memset(copybuf, 0, sizeof(copybuf));
	sceKernelDcacheWritebackInvalidateAll();
	sceDmacMemcpy(drawbuf, copybuf, sizeof(copybuf));
	sceKernelDcacheWritebackInvalidateAll();

	// To avoid text writes.
	HAS_DISPLAY = 0;
}

void setupTexture() {
	imageData[IMG_WHITE] = (u16 *)memalign(16, 8 * 8 * 2);

	u16 *p = imageData[IMG_WHITE];
	for (int i = 0; i < 8 * 8; ++i) {
		p[i] = 0xFFFF;
	}

	imageData[IMG_RED] = (u16 *)memalign(16, 8 * 8 * 2);

	p = imageData[IMG_RED];
	for (int i = 0; i < 8 * 8; ++i) {
		p[i] = 0x000F;
	}

	imageData[IMG_GRAY] = (u16 *)memalign(16, 8 * 8 * 2);

	p = imageData[IMG_GRAY];
	for (int i = 0; i < 8 * 8; ++i) {
		p[i] = 0x8888;
	}

	imageData[IMG_BROWN] = (u16 *)memalign(16, 8 * 8 * 2);

	p = imageData[IMG_BROWN];
	for (int i = 0; i < 8 * 8; ++i) {
		p[i] = 0x0525;
	}

	sceKernelDcacheWritebackInvalidateAll();
}

void testDrawingBox(const char *title, int img) {
	const void *verts = TEST_TRANSFORM ? verticesTransform : verticesThrough;

	drawTexFlush(img, verts);
	sceDisplayWaitVblankStart();
	displayBuffer(title);
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

	testDrawingBox("White", IMG_WHITE);
	testDrawingBox("Red", IMG_RED);
	testDrawingBox("Gray", IMG_GRAY);
	testDrawingBox("Brown", IMG_BROWN);

	sceGuTerm();

	free(imageData[IMG_WHITE]);
	free(imageData[IMG_RED]);
	free(imageData[IMG_GRAY]);
	free(imageData[IMG_BROWN]);

	return 0;
}
