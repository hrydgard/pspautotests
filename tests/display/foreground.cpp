#include <common.h>
#include <pspdisplay.h>
#include <pspthreadman.h>

// When sceDisplayIsForeground changes after the framebuffer is turned off or on, IMMEDIATE or
// NEXTFRAME: at once, or at the next vblank.

extern "C" int HAS_DISPLAY;

static void *const vram = (void *)0x04000000;

static void testSet(const char *title, void *topaddr, int sync) {
	int result = sceDisplaySetFrameBuf(topaddr, 512, PSP_DISPLAY_PIXEL_FORMAT_8888, sync);
	int now = sceDisplayIsForeground();
	sceDisplayWaitVblankStart();
	int after = sceDisplayIsForeground();
	sceDisplayWaitVblankStart();
	int later = sceDisplayIsForeground();
	printf("%s: %08x, foreground now=%d, after vblank=%d, after another=%d\n", title, result, now, after, later);
}

extern "C" int main(int argc, char *argv[]) {
	HAS_DISPLAY = 0;

	sceDisplaySetMode(0, 480, 272);
	sceDisplaySetFrameBuf(vram, 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
	sceDisplaySetFrameBuf(vram, 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
	sceDisplayWaitVblankStart();
	printf("Start: foreground=%d\n", sceDisplayIsForeground());

	testSet("Off, immediate", NULL, PSP_DISPLAY_SETBUF_IMMEDIATE);
	testSet("On, immediate", vram, PSP_DISPLAY_SETBUF_IMMEDIATE);
	testSet("Off, next frame", NULL, PSP_DISPLAY_SETBUF_NEXTFRAME);
	testSet("On, next frame", vram, PSP_DISPLAY_SETBUF_NEXTFRAME);

	// Off and back on before the vblank comes.
	sceDisplayWaitVblankStart();
	sceDisplaySetFrameBuf(NULL, 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
	int off = sceDisplayIsForeground();
	sceDisplaySetFrameBuf(vram, 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
	int on = sceDisplayIsForeground();
	sceDisplayWaitVblankStart();
	printf("Off and on in one frame: foreground %d, %d, after vblank=%d\n", off, on, sceDisplayIsForeground());

	return 0;
}
