// Subpixel positioning and destination blending in sceFontGetCharGlyphImage_Clip.
//
// charglyphimage and charglyphimageclip both clear the destination to zero before drawing,
// which hides the two things this test is about:
//
//   - the glyph is *added* to the buffer with saturation, it does not replace it,
//     so a glyph never erases what was already there.
//   - a fractional xPos64 spreads each column over two,
//     rounding the two halves in opposite directions.
//
// Both are invisible against a zeroed buffer,
// so everything here draws onto a non-zero fill and dumps the raw bytes afterwards.
// A byte still holding the fill was never written.

#include <common.h>
#include <pspmodulemgr.h>
#include <malloc.h>
#include <sys/types.h>  // libfont.h spells its unsigned types uint/ushort
extern "C" {
#include "libfont.h"
}

static FontLibraryHandle fontLib;
static FontHandle font;

#define BUF_W 32
#define BUF_H 20
#define PEN_X 8
#define PEN_Y 1

// Enough for the widest format: BUF_W pixels at 4 bytes each.
static u8 buf[BUF_W * 4 * BUF_H] __attribute__((aligned(16)));

static void *allocQuiet(void *arg, u32 size) {
	return malloc(size);
}

static void freeQuiet(void *arg, void *p) {
	free(p);
}

bool loadFontModule() {
	checkpointNext("Init");
	if (RUNNING_ON_EMULATOR) {
		return true;
	}

	SceUID fontModule = sceKernelLoadModule("libfont.prx", 0, NULL);
	if (fontModule <= 0) {
		printf("TEST ERROR: Unable to load libfont.prx\n");
		return false;
	}

	int status = -1;
	int result = sceKernelStartModule(fontModule, 0, NULL, &status, NULL);
	if (result != fontModule || status != 0) {
		printf("TEST ERROR: libfont.prx startup failed (%08x, %08x)\n", result, status);
		return false;
	}
	return true;
}

bool loadFontHandles() {
	FontNewLibParams libParams;
	memset(&libParams, 0, sizeof(libParams));
	libParams.allocFuncAddr = allocQuiet;
	libParams.freeFuncAddr = freeQuiet;
	libParams.numFonts = 4;

	uint error = -1;
	fontLib = sceFontNewLib(&libParams, &error);

	font = sceFontOpenUserFile(fontLib, "ltn0.pgf", 1, &error);
	if (font <= 0) {
		printf("TEST ERROR: Unable to load ltn0.pgf\n");
		return false;
	}
	return true;
}

static int bytesPerLineFor(int pixelFormat) {
	switch (pixelFormat) {
	case PSP_FONT_PIXELFORMAT_4:
	case PSP_FONT_PIXELFORMAT_4_REV: return BUF_W / 2;
	case PSP_FONT_PIXELFORMAT_24:    return BUF_W * 3;
	case PSP_FONT_PIXELFORMAT_32:    return BUF_W * 4;
	default:                         return BUF_W;
	}
}

// Draw one glyph onto a buffer pre-filled with `fill` and dump the raw bytes.
static void testDraw(const char *title, u16 charCode, int xFrac, int yFrac, int pixelFormat, u8 fill) {
	GlyphImage glyph;
	int bytesPerLine = bytesPerLineFor(pixelFormat);

	memset(buf, fill, sizeof(buf));

	glyph.pixelFormat = pixelFormat;
	glyph.positionX_F26_6 = (PEN_X << 6) + xFrac;
	glyph.positionY_F26_6 = (PEN_Y << 6) + yFrac;
	glyph.bufferWidth = BUF_W;
	glyph.bufferHeight = BUF_H;
	glyph.bytesPerLine = bytesPerLine;
	glyph.__padding = 0;
	glyph.buffer = buf;

	int result = sceFontGetCharGlyphImage_Clip(font, charCode, &glyph, 0, 0, BUF_W, BUF_H);
	if (result != 0) {
		checkpoint("%s: Failed (%08x)", title, result);
		return;
	}

	checkpoint("%s: OK", title);
	for (int y = 0; y < BUF_H; ++y) {
		const u8 *p = buf + y * bytesPerLine;
		schedf("   -> ");
		for (int i = 0; i < bytesPerLine; ++i) {
			schedf("%02x", p[i]);
		}
		schedf("\n");
	}
}

static void reportCharInfo(const char *title, u16 charCode) {
	FontCharInfo ci;
	memset(&ci, 0, sizeof(ci));

	int result = sceFontGetCharInfo(font, charCode, &ci);
	if (result != 0) {
		checkpoint("%s: Failed (%08x)", title, result);
		return;
	}

	// Just the bitmap placement - the metrics are charinfo's job.
	// bitmapWidth is what says which column is the one past the glyph.
	checkpoint("%s: bitmap %dx%d, from %d,%d", title,
		(int)ci.bitmapWidth, (int)ci.bitmapHeight, (int)ci.bitmapLeft, (int)ci.bitmapTop);
}

// Which formats libfont will actually render into.
// Only two of the five do anything: the rest return 0 and leave the buffer completely untouched.
void testPixelFormats() {
	checkpointNext("Pixel formats:");
	testDraw("  4bpp", 'v', 32, 0, PSP_FONT_PIXELFORMAT_4, 0x44);
	testDraw("  4bpp reversed", 'v', 32, 0, PSP_FONT_PIXELFORMAT_4_REV, 0x44);
	testDraw("  8bpp", 'v', 32, 0, PSP_FONT_PIXELFORMAT_8, 0x44);
	testDraw("  24bpp", 'v', 32, 0, PSP_FONT_PIXELFORMAT_24, 0x44);
	testDraw("  32bpp", 'v', 32, 0, PSP_FONT_PIXELFORMAT_32, 0x44);
}

// A fractional xPos64 splits each glyph column across two destination columns,
// so the drawn area is one column wider than the glyph.
// 'v' has rows whose last column is blank and 'i' is only a few pixels wide,
// which puts that extra column in two very different places.
void testXFractions() {
	static const int fracs[] = { 0, 16, 32, 48, 63 };

	checkpointNext("X fractions:");
	for (int i = 0; i < (int)ARRAY_SIZE(fracs); ++i) {
		char title[32];
		snprintf(title, sizeof(title), "  'v' + %d/64", fracs[i]);
		testDraw(title, 'v', fracs[i], 0, PSP_FONT_PIXELFORMAT_4, 0x44);
	}
	for (int i = 0; i < (int)ARRAY_SIZE(fracs); ++i) {
		char title[32];
		snprintf(title, sizeof(title), "  'i' + %d/64", fracs[i]);
		testDraw(title, 'i', fracs[i], 0, PSP_FONT_PIXELFORMAT_4, 0x44);
	}
}

// The vertical fraction is discarded: these five should be identical to each other, with no extra row below the glyph.
void testYFractions() {
	static const int fracs[] = { 0, 16, 32, 48, 63 };

	checkpointNext("Y fractions:");
	for (int i = 0; i < (int)ARRAY_SIZE(fracs); ++i) {
		char title[32];
		snprintf(title, sizeof(title), "  'v' + %d/64", fracs[i]);
		testDraw(title, 'v', 32, fracs[i], PSP_FONT_PIXELFORMAT_4, 0x44);
	}
}

// How the glyph combines with what is already in the buffer.
// Against 00 the result is the glyph itself; against ff everything the glyph touches should stay ff,
// which no plain store could produce.
void testDestination() {
	static const u8 fills[] = { 0x00, 0x44, 0x77, 0xff };

	checkpointNext("Destination:");
	for (int i = 0; i < (int)ARRAY_SIZE(fills); ++i) {
		char title[32];
		snprintf(title, sizeof(title), "  4bpp onto %02x", fills[i]);
		testDraw(title, 'v', 32, 0, PSP_FONT_PIXELFORMAT_4, fills[i]);
	}
	for (int i = 0; i < (int)ARRAY_SIZE(fills); ++i) {
		char title[32];
		snprintf(title, sizeof(title), "  8bpp onto %02x", fills[i]);
		testDraw(title, 'v', 32, 0, PSP_FONT_PIXELFORMAT_8, fills[i]);
	}
}

extern "C" int main(int argc, char *argv[]) {
	if (!loadFontModule()) {
		return 1;
	}
	if (!loadFontHandles()) {
		return 1;
	}

	checkpointNext("Glyphs:");
	reportCharInfo("  'v'", 'v');
	reportCharInfo("  'i'", 'i');

	testPixelFormats();
	testXFractions();
	testYFractions();
	testDestination();

	sceFontClose(font);
	sceFontDoneLib(fontLib);

	return 0;
}
