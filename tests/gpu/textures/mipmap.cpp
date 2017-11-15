#include <common.h>
#include <malloc.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>

extern "C" int sceDmacMemcpy(void *dest, const void *source, unsigned int size);
extern "C" void sendCommandi(int cmd, int argument);

extern int HAS_DISPLAY;

// Test reversed and equal mip level behavior.
const bool TEST_UNEVEN_MIPS = false;
// Test linear behavior, or show rounding?
const bool TEST_LINEAR_MIPS = true;
// Through mode or transform?
const bool TEST_TRANSFORM = false;

u8 *fbp0 = 0;
u8 *dbp0 = fbp0 + 512 * 272 * sizeof(u32);

static u32 copybuf[512 * 272];
u32 *drawbuf;

unsigned int __attribute__((aligned(16))) list[262144];

enum {
	IMG_HALVES,
	IMG_EQUAL,
	IMG_REVERSE,
};

u32 *imageData[3] = {};
u32 imageColors[] = {
	0xFF000000,
	0xFF101010,
	0xFF202020,
	0xFF303030,
	0xFF404040,
	0xFF505050,
	0xFF606060,
	0xFF707070,
};

typedef struct {
	u16 u, v;
	s16 x, y, z;
} Vertex;

enum {
	VERTS_EXACT,
	VERTS_MAGNIFY,
	VERTS_2X,
	VERTS_4X_W,
	VERTS_4X_H,
	VERTS_64X,
	VERTS_256X,
};
inline s16 norm16x(int x) { return x * 65536 / 480 - 32768; }
inline s16 norm16y(int x) { return 32767 - x * 65536 / 272; }

Vertex verticesThrough[][2] = {
	{ {0, 0, 0, 0, 0}, {2, 2, 2, 2, 0} },
	{ {0, 0, 0, 0, 0}, {2, 2, 8, 8, 0} },
	{ {0, 0, 0, 0, 0}, {4, 4, 2, 2, 0} },
	{ {0, 0, 0, 0, 0}, {8, 1, 2, 2, 0} },
	{ {0, 0, 0, 0, 0}, {1, 8, 2, 2, 0} },
	{ {0, 0, 0, 0, 0}, {64, 64, 1, 1, 0} },
	{ {0, 0, 0, 0, 0}, {256, 256, 1, 1, 0} },
};
Vertex verticesTransform[][2] = {
	{ {0, 0, norm16x(0), norm16y(0), 65500}, {256, 256, norm16x(2), norm16y(2), 65535} },
	{ {0, 0, norm16x(0), norm16y(0), 65500}, {256, 256, norm16x(8), norm16y(8), 65535} },
	{ {0, 0, norm16x(0), norm16y(0), 65500}, {512, 512, norm16x(2), norm16y(2), 65535} },
	{ {0, 0, norm16x(0), norm16y(0), 65500}, {1024, 128, norm16x(2), norm16y(2), 65535} },
	{ {0, 0, norm16x(0), norm16y(0), 65500}, {128, 1024, norm16x(2), norm16y(2), 65535} },
	{ {0, 0, norm16x(0), norm16y(0), 65500}, {8192, 8192, norm16x(1), norm16y(1), 65535} },
	{ {0, 0, norm16x(0), norm16y(0), 65500}, {32768, 32768, norm16x(1), norm16y(1), 65535} },
};

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

u32 *getTexturePtr(int img, int l) {
	int p = 0;
	int w;
	if (img == IMG_HALVES) {
		w = 256;
	} else if (img == IMG_EQUAL) {
		w = 16;
	} else if (img == IMG_REVERSE) {
		w = 4;
	}

	while (l-- > 0) {
		p += w * w;
		if ((p % 4) != 0) {
			p += 4 - (p % 4);
		}

		if (img == IMG_HALVES) {
			w >>= 1;
		} else if (img == IMG_EQUAL) {
			w = 16;
		} else if (img == IMG_REVERSE) {
			w <<= 1;
		}
	}

	return imageData[img] + p;
}

void drawTexFlush(int img, unsigned int mode, const void *verts, u8 bias) {
	sceGuStart(GU_DIRECT, list);

	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexSync();
	sceGuTexFlush();
	sendCommandi(200, (bias << 16) | mode);
	sceGuTexMode(GU_PSM_8888, 7, 0, GU_FALSE);
	sceGuTexWrap(GU_CLAMP, GU_CLAMP);
	sceGuTexFunc(GU_TFX_DECAL, GU_TCC_RGB);
	sceGuTexSlope(2.0f);
	for (int l = 0; l < 8; ++l) {
		int w;
		if (img == IMG_HALVES) {
			w = 256 >> l;
		} else if (img == IMG_EQUAL) {
			w = 16;
		} else if (img == IMG_REVERSE) {
			w = 4 << l;
		}

		sceGuTexImage(l, w, w, w < 4 ? 4 : w, getTexturePtr(img, l));
	}

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
	imageData[IMG_HALVES] = (u32 *)memalign(16, 4 * 256 * 344);

	int w = 256;
	for (int l = 0; l < 8; ++l) {
		u32 *p = getTexturePtr(IMG_HALVES, l);
		int bufw = w < 4 ? 4 : w;
		for (int i = 0; i < bufw * w; ++i) {
			p[i] = imageColors[l];
		}
		w >>= 1;
	}

	imageData[IMG_EQUAL] = (u32 *)memalign(16, 4 * 16 * 16 * 8);

	w = 16;
	for (int l = 0; l < 8; ++l) {
		u32 *p = getTexturePtr(IMG_EQUAL, l);
		int bufw = w < 4 ? 4 : w;
		for (int i = 0; i < bufw * w; ++i) {
			p[i] = imageColors[l];
		}
	}

	imageData[IMG_REVERSE] = (u32 *)memalign(16, 4 * 512 * 1024);

	w = 4;
	for (int l = 0; l < 8; ++l) {
		u32 *p = getTexturePtr(IMG_REVERSE, l);
		int bufw = w < 4 ? 4 : w;
		for (int i = 0; i < bufw * w; ++i) {
			p[i] = imageColors[l];
		}
		w <<= 1;
	}

	sceKernelDcacheWritebackInvalidateAll();
}

void testMipDrawingBox(const char *title, int img, unsigned int mode, int vindex, u8 bias) {
	const void *verts = TEST_TRANSFORM ? verticesTransform[vindex] : verticesThrough[vindex];

	drawTexFlush(img, mode, verts, bias);
	sceDisplayWaitVblankStart();
	displayBuffer(title);
}

void testMipDrawing(const char *title, int img, unsigned int mode, u8 bias) {
	char full[1024] = {0};
	snprintf(full, sizeof(full) - 1, "%s +%02x:", title, bias);

	checkpointNext(full);
	testMipDrawingBox("  1:1", img, mode, VERTS_EXACT, bias);
	testMipDrawingBox("  Magnify", img, mode, VERTS_MAGNIFY, bias);
	testMipDrawingBox("  Minify 2x WH", img, mode, VERTS_2X, bias);
	testMipDrawingBox("  Minify 4x W", img, mode, VERTS_4X_W, bias);
	testMipDrawingBox("  Minify 4x H", img, mode, VERTS_4X_H, bias);
	testMipDrawingBox("  Minify 64x", img, mode, VERTS_64X, bias);
	testMipDrawingBox("  Minify 256x", img, mode, VERTS_256X, bias);
}

extern "C" int main(int argc, char *argv[]) {
	init();
	setupTexture();

	sceDisplaySetFrameBuf(sceGeEdramGetAddr(), 512, GU_PSM_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
	sceDisplaySetMode(0, 480, 272);

	sceGuStart(GU_DIRECT, list);
	if (TEST_LINEAR_MIPS) {
		sceGuTexFilter(GU_NEAREST_MIPMAP_LINEAR, GU_NEAREST_MIPMAP_LINEAR);
	} else {
		sceGuTexFilter(GU_NEAREST_MIPMAP_NEAREST, GU_NEAREST_MIPMAP_NEAREST);
	}
	sceGuFinish();
	sceGuSync(0, 0);

	u8 biases[] = {0x00, 0x07, 0x08, 0x10, 0x70, 0x77, 0x78, 0x80, 0x87, 0x88, 0xF0};
	for (size_t i = 0; i < ARRAY_SIZE(biases); ++i) {
		testMipDrawing("Typical mips (AUTO)", IMG_HALVES, GU_TEXTURE_AUTO, biases[i]);
		if (TEST_UNEVEN_MIPS) {
			testMipDrawing("Equal mips (AUTO)", IMG_EQUAL, GU_TEXTURE_AUTO, biases[i]);
			testMipDrawing("Reversed mips (AUTO)", IMG_REVERSE, GU_TEXTURE_AUTO, biases[i]);
		}
	}

	for (size_t i = 0; i < ARRAY_SIZE(biases); ++i) {
		testMipDrawing("Typical mips (CONST)", IMG_HALVES, GU_TEXTURE_CONST, biases[i]);
		if (TEST_UNEVEN_MIPS) {
			testMipDrawing("Equal mips (CONST)", IMG_EQUAL, GU_TEXTURE_CONST, biases[i]);
			testMipDrawing("Reversed mips (CONST)", IMG_REVERSE, GU_TEXTURE_CONST, biases[i]);
		}
	}

	for (size_t i = 0; i < ARRAY_SIZE(biases); ++i) {
		testMipDrawing("Typical mips (SLOPE)", IMG_HALVES, GU_TEXTURE_SLOPE, biases[i]);
		if (TEST_UNEVEN_MIPS) {
			testMipDrawing("Equal mips (SLOPE)", IMG_EQUAL, GU_TEXTURE_SLOPE, biases[i]);
			testMipDrawing("Reversed mips (SLOPE)", IMG_REVERSE, GU_TEXTURE_SLOPE, biases[i]);
		}
	}

	for (size_t i = 0; i < ARRAY_SIZE(biases); ++i) {
		testMipDrawing("Typical mips (3)", IMG_HALVES, 3, biases[i]);
		if (TEST_UNEVEN_MIPS) {
			testMipDrawing("Equal mips (3)", IMG_EQUAL, 3, biases[i]);
			testMipDrawing("Reversed mips (3)", IMG_REVERSE, 3, biases[i]);
		}
	}

	sceGuTerm();
	
	free(imageData[IMG_HALVES]);
	free(imageData[IMG_EQUAL]);
	free(imageData[IMG_REVERSE]);

	return 0;
}
