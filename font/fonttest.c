#include <pspsdk.h>
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include "libfont.h"

PSP_MODULE_INFO("font test", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define eprintf(...) pspDebugScreenPrintf(__VA_ARGS__); Kprintf(__VA_ARGS__);

static void *Font_Alloc(void *data, u32 size) {
	eprintf("Font_Alloc(%08X, %d)\n", (uint)data, size);
	return malloc(size);
}

static void Font_Free(void *data, void *p){
	eprintf("Font_Free(%08X, %08X)\n", (uint)data, (uint)p);
	free(p);
}

int main(int argc, char *argv[]) {
	FontLibraryHandle libHandle;
	FontHandle        fontHandle;
	FontInfo          fontInfo;
	int result;
	u32 errorCode;
	FontNewLibParams params = { NULL, 4, NULL, Font_Alloc, Font_Free, NULL, NULL, NULL, NULL, NULL, NULL };

	pspDebugScreenInit();
	
	eprintf("Starting fonttest...\n");

	libHandle = sceFontNewLib(&params, &errorCode);
	eprintf("sceFontNewLib: %d, %08X\n", libHandle != 0, errorCode);
	
	fontHandle = sceFontOpen(libHandle, 0, 0777, &errorCode);
	eprintf("sceFontOpen: %d, %08X\n", fontHandle != 0, errorCode);
	
	/*
	result = sceFontGetFontInfo(fontHandle, &fontInfo);
	eprintf("sceFontGetFontInfo: %d\n", result);
	
	#define PRINT_GLYPH_METRICS_MAX(TYPE) eprintf("fontInfo.max" #TYPE ": %d, %f\n", fontInfo.max## TYPE ## I, fontInfo.max## TYPE ## F);
	#define PRINT_GLYPH_METRICS_MIN(TYPE) eprintf("fontInfo.min" #TYPE ": %d, %f\n", fontInfo.min## TYPE ## I, fontInfo.min## TYPE ## F);
	
	PRINT_GLYPH_METRICS_MAX(GlyphWidth);
	PRINT_GLYPH_METRICS_MAX(GlyphHeight);
	PRINT_GLYPH_METRICS_MAX(GlyphAscender);
	PRINT_GLYPH_METRICS_MAX(GlyphDescender);
	PRINT_GLYPH_METRICS_MAX(GlyphLeftX);
	PRINT_GLYPH_METRICS_MAX(GlyphBaseY);
	PRINT_GLYPH_METRICS_MIN(GlyphCenterX);
	PRINT_GLYPH_METRICS_MAX(GlyphTopY);
	PRINT_GLYPH_METRICS_MAX(GlyphAdvanceX);
	PRINT_GLYPH_METRICS_MAX(GlyphAdvanceY);
	*/

	return 0;
}