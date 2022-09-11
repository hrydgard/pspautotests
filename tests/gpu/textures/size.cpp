#include <common.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>
#include "../commands/commands.h"

extern int HAS_DISPLAY;

extern "C" int sceDmacMemcpy(void *dest, const void *source, unsigned int size);

static u8 *fbp0 = 0;
static u8 *dbp0 = fbp0 + 512 * 272 * sizeof(u32);

static u32 copybuf[512 * 272];
static u32 *drawbuf;

static u32 __attribute__((aligned(16))) list[32768];
static u32 __attribute__((aligned(16))) texbuf[32768 * 4];
static u32 __attribute__((aligned(16))) clut[256];

struct Vertex {
	Vertex() {}
	Vertex(u16 uu, u16 vv, s16 xx, s16 yy, s16 zz = 0) : u(uu), v(vv), x(xx), y(yy), z(zz) {}

	u16 u, v;
	s16 x, y, z;
};

static void init() {
	void *fbp0 = 0;

	drawbuf = (u32 *)sceGeEdramGetAddr();

	sceGuInit();
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888, fbp0, 512);
	sceGuDispBuffer(480, 272, fbp0, 512);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuScissor(0, 0, 480, 272);
	sceGuOffset(2048 - (256 / 2), 2048 - (256 / 2));
	sceGuViewport(2048, 2048, 256, 256);
	sceGuDepthMask(GU_TRUE);
	sceGuFrontFace(GU_CW);
	sceGuShadeModel(GU_SMOOTH);
	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	sceGuTexFilter(GU_NEAREST, GU_NEAREST);
	sceGuTexLevelMode(GU_TEXTURE_CONST, 0.0f);
	sceGuTexWrap(GU_CLAMP, GU_CLAMP);
	//sceGuTexWrap(GU_REPEAT, GU_REPEAT);

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
	sceKernelDcacheWritebackRange(copybuf, sizeof(copybuf));
	sceDmacMemcpy(drawbuf, copybuf, sizeof(copybuf));
}

static bool correctedGuTexImage(int level, int wf, int hf, int bufw, const void *texptr) {
	if (level < 0 || level > 7)
		return false;
	if (wf < 0 || hf < 0 || wf > 0xFF || hf > 0xFF)
		return false;
	if (bufw < 0 || bufw > 0xFFFF)
		return false;

	sceGuSendCommandi(GE_CMD_TEXADDR0 + level, (intptr_t)texptr & 0x00FFFFFF);
	sceGuSendCommandi(GE_CMD_TEXBUFWIDTH0 + level, (((intptr_t)texptr & 0xFF000000) >> 8) | bufw);
	sceGuSendCommandi(GE_CMD_TEXSIZE0 + level, (hf << 8) | wf);
	return true;
}

static void prepareTexture(int fmt, int wf, int hf, int w, int h) {
	int bufw;
	if (fmt == GU_PSM_8888) {
		bufw = w < 4 ? 4 : w;
		for (int i = 0; i < w * h; ++i) {
			texbuf[i] = 0xFFF00000 | i;
		}
		sceKernelDcacheWritebackRange(texbuf, sizeof(texbuf));
	} else if (fmt == GU_PSM_4444) {
		u16 *texbuf16 = (u16 *)texbuf;
		bufw = w < 8 ? 8 : w;
		for (int i = 0; i < w * h; ++i) {
			texbuf16[i] = 0xF000 | (i & 0x0FFF);
		}
		sceKernelDcacheWritebackRange(texbuf, sizeof(texbuf));
	} else if (fmt == GU_PSM_T8) {
		u8 *texbuf8 = (u8 *)texbuf;
		bufw = w < 16 ? 16 : w;
		for (int i = 0; i < w * h; ++i) {
			texbuf8[i] = (i % w) & 0xFF;
		}
		sceKernelDcacheWritebackRange(texbuf, sizeof(texbuf));

		for (int i = 0; i < 256; ++i) {
			clut[i] = 0xFF000000 | i;
		}
		sceKernelDcacheWritebackRange(clut, sizeof(clut));
		sceGuClutLoad(32, clut);
		sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
	} else if (fmt == GU_PSM_T4) {
		u8 *texbuf8 = (u8 *)texbuf;
		bufw = w < 32 ? 32 : w;
		for (int i = 0; i < w * h; i += 2) {
			texbuf8[i / 2] = (i & 0xF) | (((i + 1) << 4) & 0xF0);
		}
		sceKernelDcacheWritebackRange(texbuf, sizeof(texbuf));

		for (int i = 0; i < 256; ++i) {
			clut[i] = 0xFF000000 | i;
		}
		sceKernelDcacheWritebackRange(clut, sizeof(clut));
		sceGuClutLoad(32, clut);
		sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
	}

	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexMode(fmt, 0, 0, GU_FALSE);
	correctedGuTexImage(0, wf, hf, bufw, texbuf);
	sceGuTexFlush();
	sceGuTexSync();
}

static const char *formatTitle(int fmt) {
	if (fmt == GU_PSM_8888) {
		return "8888";
	} else if (fmt == GU_PSM_4444) {
		return "4444";
	} else if (fmt == GU_PSM_T8) {
		return "CLUT8";
	} else if (fmt == GU_PSM_T4) {
		return "CLUT4";
	}
	return "?";
}

static void testTextureFormat(const char *title, int fmt, bool through, int wf, int hf) {
	int w = 1 << wf;
	int h = 1 << hf;
	if (w * h > ARRAY_SIZE(texbuf)) {
		checkpoint("TEST ERROR: invalid w/h for %s", title);
		return;
	}

	sceGuStart(GU_DIRECT, list);
	prepareTexture(fmt, wf, hf, w, h);

	Vertex verts[2];
	verts[0] = Vertex(0, 0, through ? 0 : -32768, through ? 0 : 32767);
	verts[1] = Vertex(through ? w : 32768, through ? h : 32768, through ? 256 : 32767, through ? 256 : -32768);
	sceKernelDcacheWritebackRange(verts, sizeof(verts));

	int transform = through ? GU_TRANSFORM_2D : GU_TRANSFORM_3D;
	sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | transform, 2, NULL, verts);

	sceGuFinish();
	sceGuSync(0, 0);

	sceKernelDcacheWritebackInvalidateRange(copybuf, sizeof(copybuf));
	sceDmacMemcpy(copybuf, drawbuf, sizeof(copybuf));
	sceKernelDcacheWritebackInvalidateRange(copybuf, sizeof(copybuf));

	u32 tl = copybuf[0];
	u32 tc = copybuf[127];
	u32 tr = copybuf[255];
	u32 cl = copybuf[127 * 512];
	u32 bl = copybuf[255 * 512];
	u32 br = copybuf[255 * 512 + 255];
	const char *format = formatTitle(fmt);
	if (hf == 0) {
		checkpoint("%s %s %s: TL=%06x TC=%06x, TR=%06x, BR=%06x", title, format, through ? "through" : "transform", tl, tc, tr, br);
	} else if (wf == 0) {
		checkpoint("%s %s %s: TL=%06x CL=%06x, BL=%06x, BR=%06x", title, format, through ? "through" : "transform", tl, cl, bl, br);
	} else {
		checkpoint("%s %s %s: TL=%06x TC=%06x, TR=%06x, CL=%06x, BL=%06x, BR=%06x", title, format, through ? "through" : "transform", tl, tc, tr, cl, bl, br);
	}
}

extern "C" int main(int argc, char *argv[]) {
	init();

	sceDisplaySetFrameBuf(sceGeEdramGetAddr(), 512, GU_PSM_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
	sceDisplaySetMode(0, 480, 272);
	HAS_DISPLAY = 0;

	checkpointNext("Texture widths:");
	static const int fmts[] = { GU_PSM_8888, GU_PSM_4444, GU_PSM_T8, GU_PSM_T4 };
	for (size_t f = 0; f < ARRAY_SIZE(fmts); ++f) {
		for (int i = 0; i < 16; ++i) {
			char temp[128];
			sprintf(temp, "  Texture w=%d", 1 << i);
			testTextureFormat(temp, fmts[f], false, i, 0);
		}
		for (int i = 0; i < 16; ++i) {
			char temp[128];
			sprintf(temp, "  Texture w=%d", 1 << i);
			testTextureFormat(temp, fmts[f], true, i, 0);
		}
	}

	checkpointNext("Texture heights:");
	for (size_t f = 0; f < ARRAY_SIZE(fmts); ++f) {
		for (int i = 0; i < 16; ++i) {
			char temp[128];
			sprintf(temp, "  Texture h=%d", 1 << i);
			testTextureFormat(temp, fmts[f], false, 0, i);
		}
		for (int i = 0; i < 16; ++i) {
			char temp[128];
			sprintf(temp, "  Texture h=%d", 1 << i);
			testTextureFormat(temp, fmts[f], true, 0, i);
		}
	}

	sceGuTerm();

	return 0;
}