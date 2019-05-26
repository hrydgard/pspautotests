#include "../commands/shared.h"
#include "../commands/commands.h"

static u16 __attribute__((aligned(16))) copybuf[512 * 272];
static u16 __attribute__((aligned(16))) resultbuf[32];

static u32 from565(int c) {
	int r = (c & 0x001F) << 3;
	int g = (c & 0x07E0) << 5;
	int b = (c & 0xF800) << 8;
	return r | g | b;
}

static u32 from5551(int c) {
	int r = (c & 0x001F) << 3;
	int g = (c & 0x03E0) << 6;
	int b = (c & 0x7C00) << 9;
	int a = (c & 0x8000) << 16;
	return r | g | b | a;
}

static u32 from4444(int c) {
	int r = (c & 0x000F) << 4;
	int g = (c & 0x00F0) << 8;
	int b = (c & 0x0F00) << 12;
	int a = (c & 0xF000) << 16;
	return r | g | b | a;
}

static void printRow(const char *title, int row, int fmt) {
	u16 *c = &resultbuf[row * 4];
	if (fmt == GU_PSM_5650) {
		checkpoint("  %s %d: %04x %04x %04x %04x / %08x %08x %08x %08x", title, row, c[0], c[1], c[2], c[3], from565(c[0]), from565(c[1]), from565(c[2]), from565(c[3]));
	} else if (fmt == GU_PSM_5551) {
		checkpoint("  %s %d: %04x %04x %04x %04x / %08x %08x %08x %08x", title, row, c[0], c[1], c[2], c[3], from5551(c[0]), from5551(c[1]), from5551(c[2]), from5551(c[3]));
	} else if (fmt == GU_PSM_4444) {
		checkpoint("  %s %d: %04x %04x %04x %04x / %08x %08x %08x %08x", title, row, c[0], c[1], c[2], c[3], from4444(c[0]), from4444(c[1]), from4444(c[2]), from4444(c[3]));
	} else {
		u32 *c32 = (u32 *)&resultbuf[row * 8];
		checkpoint("  %s %d: %08x %08x %08x %08x", title, row, c32[0], c32[1], c32[2], c32[3]);
	}
}

static void draw(const char *title, const ScePspIVector4 *dither, int fmt, int xoff = 0, int yoff = 0, bool stencil = false, u32 cmask = 0, bool logic = false, bool clear = false) {
	sceGuDrawBuffer(fmt, fbp0, 512);
	sceGuStart(GU_DIRECT, list);
	sceGuEnable(GU_DITHER);
	sceGuDisable(GU_TEXTURE_2D);
	sceGuDisable(GU_BLEND);
	if (stencil) {
		sceGuEnable(GU_STENCIL_TEST);
		sceGuStencilFunc(GU_ALWAYS, 0xF0, 0xFF);
		sceGuStencilOp(GU_REPLACE, GU_REPLACE, GU_REPLACE);
	} else {
		sceGuDisable(GU_STENCIL_TEST);
	}
	sceGuPixelMask(cmask);
	if (logic) {
		sceGuEnable(GU_COLOR_LOGIC_OP);
		sceGuLogicalOp(GU_AND);
		memset(copybuf, 0x77, sizeof(copybuf));
	} else {
		sceGuDisable(GU_COLOR_LOGIC_OP);
		memset(copybuf, 0x00, sizeof(copybuf));
	}
	if (clear) {
		sceGuSendCommandi(GE_CMD_CLEARMODE, 0xFFFF);
	} else {
		sceGuSendCommandi(GE_CMD_CLEARMODE, 0);
	}

	int bpp = fmt == GU_PSM_8888 ? 4 : 2;
	int mult = bpp / 2;

	sceKernelDcacheWritebackInvalidateRange(copybuf, sizeof(copybuf));
	sceDmacMemcpy(sceGeEdramGetAddr(), copybuf, sizeof(copybuf));

	sceGuSendCommandi(GE_CMD_DITH0, dither->x);
	sceGuSendCommandi(GE_CMD_DITH1, dither->y);
	sceGuSendCommandi(GE_CMD_DITH2, dither->z);
	sceGuSendCommandi(GE_CMD_DITH3, dither->w);

	Vertices v(GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D);
	v.CP(0xFF1FF088, 0 + xoff, 0 + yoff, 0);
	v.CP(0xFF1FF088, 480, 0 + yoff, 0);
	v.CP(0xFF1FF088, 480, 272, 0);
	v.CP(0xFF1FF088, 0 + xoff, 272, 0);

	void *p = sceGuGetMemory(v.Size());
	memcpy(p, v.Ptr(), v.Size());
	sceGuDrawArray(GU_TRIANGLE_FAN, v.VType(), 4, NULL, p);

	sceGuFinish();
	sceGuSync(0, 0);

	sceKernelDcacheWritebackInvalidateRange(sceGeEdramGetAddr(), sizeof(copybuf));
	sceDmacMemcpy(copybuf, sceGeEdramGetAddr(), sizeof(copybuf));
	memcpy(resultbuf + 4 * mult * 0, copybuf + 512 * mult * (0 + yoff) + xoff * mult, 4 * bpp);
	memcpy(resultbuf + 4 * mult * 1, copybuf + 512 * mult * (1 + yoff) + xoff * mult, 4 * bpp);
	memcpy(resultbuf + 4 * mult * 2, copybuf + 512 * mult * (2 + yoff) + xoff * mult, 4 * bpp);
	memcpy(resultbuf + 4 * mult * 3, copybuf + 512 * mult * (3 + yoff) + xoff * mult, 4 * bpp);
	sceKernelDcacheWritebackInvalidateRange(resultbuf, sizeof(resultbuf));

	checkpointNext(title);
	printRow(title, 0, fmt);
	printRow(title, 1, fmt);
	printRow(title, 2, fmt);
	printRow(title, 3, fmt);
}

extern "C" int main(int argc, char *argv[]) {
	initDisplay();

	static const ScePspIVector4 zeroMatrix = {
		0x0000,
		0x0000,
		0x0000,
		0x0000,
	};
	draw("Zero matrix 565", &zeroMatrix, GU_PSM_5650);
	draw("Zero matrix 5551", &zeroMatrix, GU_PSM_5551);
	draw("Zero matrix 4444", &zeroMatrix, GU_PSM_4444);

	static const ScePspIVector4 topLeftFMatrix = {
		0x000F,
		0x0000,
		0x0000,
		0x0000,
	};
	draw("Top left -1 565", &topLeftFMatrix, GU_PSM_5650);
	draw("Top left -1 5551", &topLeftFMatrix, GU_PSM_5551);
	draw("Top left -1 4444", &topLeftFMatrix, GU_PSM_4444);

	static const ScePspIVector4 topLeft8Matrix = {
		0x0008,
		0x0000,
		0x0000,
		0x0000,
	};
	draw("Top left -8 565", &topLeft8Matrix, GU_PSM_5650);
	draw("Top left -8 5551", &topLeft8Matrix, GU_PSM_5551);
	draw("Top left -8 4444", &topLeft8Matrix, GU_PSM_4444);

	static const ScePspIVector4 topLeft7Matrix = {
		0x0007,
		0x0000,
		0x0000,
		0x0000,
	};
	draw("Top left 7 565", &topLeft7Matrix, GU_PSM_5650);
	draw("Top left 7 5551", &topLeft7Matrix, GU_PSM_5551);
	draw("Top left 7 4444", &topLeft7Matrix, GU_PSM_4444);

	static const ScePspIVector4 allMatrix = {
		0x3210,
		0x7654,
		0xBA98,
		0xFEDC,
	};
	draw("All pattern 565", &allMatrix, GU_PSM_5650);
	draw("All pattern 5551", &allMatrix, GU_PSM_5551);
	draw("All pattern 4444", &allMatrix, GU_PSM_4444);

	draw("Offset +1/+1 565", &allMatrix, GU_PSM_5650, 1, 1);
	draw("Offset +1/+1 5551", &allMatrix, GU_PSM_5551, 1, 1);
	draw("Offset +1/+1 4444", &allMatrix, GU_PSM_4444, 1, 1);

	draw("Offset +476/+268 565", &allMatrix, GU_PSM_5650, 476, 268);
	draw("Offset +476/+268 5551", &allMatrix, GU_PSM_5551, 476, 268);
	draw("Offset +476/+268 4444", &allMatrix, GU_PSM_4444, 476, 268);

	draw("Offset +1/+1 + stencil 565", &allMatrix, GU_PSM_5650, 1, 1, true);
	draw("Offset +1/+1 + stencil 5551", &allMatrix, GU_PSM_5551, 1, 1, true);
	draw("Offset +1/+1 + stencil 4444", &allMatrix, GU_PSM_4444, 1, 1, true);

	draw("Mask bits high 565", &allMatrix, GU_PSM_5650, 0, 0, false, 0xFF0F0F0F);
	draw("Mask bits high 5551", &allMatrix, GU_PSM_5551, 0, 0, false, 0xFF0F0F0F);
	draw("Mask bits high 4444", &allMatrix, GU_PSM_4444, 0, 0, false, 0xFF0F0F0F);

	draw("Logic op 565", &allMatrix, GU_PSM_5650, 0, 0, false, 0, true);
	draw("Logic op 5551", &allMatrix, GU_PSM_5551, 0, 0, false, 0, true);
	draw("Logic op 4444", &allMatrix, GU_PSM_4444, 0, 0, false, 0, true);

	draw("All pattern 8888", &allMatrix, GU_PSM_8888);

	static const ScePspIVector4 overflowMatrix = {
		0xFF3210,
		0xFF7654,
		0xFFBA98,
		0xFFFEDC,
	};
	draw("Bit overflow 565", &overflowMatrix, GU_PSM_5650);
	draw("Bit overflow 5551", &overflowMatrix, GU_PSM_5551);
	draw("Bit overflow 4444", &overflowMatrix, GU_PSM_4444);
	draw("Bit overflow 8888", &overflowMatrix, GU_PSM_8888);

	draw("Clear mode 565", &overflowMatrix, GU_PSM_5650, 0, 0, false, 0, false, true);
	draw("Clear mode 5551", &overflowMatrix, GU_PSM_5551, 0, 0, false, 0, false, true);
	draw("Clear mode 4444", &overflowMatrix, GU_PSM_4444, 0, 0, false, 0, false, true);
	draw("Clear mode 8888", &allMatrix, GU_PSM_8888, 0, 0, false, 0, false, true);

	sceGuTerm();

	return 0;
}