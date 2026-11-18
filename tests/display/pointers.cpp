#include <common.h>
#include <pspdisplay.h>
#include <pspthreadman.h>
#include <psputils.h>

// The getters' out pointers (NULL, kernel and uncached addresses from user mode), and which
// framebuffer addresses sceDisplaySetFrameBuf accepts.

extern "C" {
int sceDisplayGetBrightness(int *level, int *unk);
int sceDisplayGetResumeMode(int *mode);
}

extern "C" int HAS_DISPLAY;

static void *const vram = (void *)0x04000000;
static const u32 UNSET = 0x11111111;

static u32 a, b, c;

static void reset() {
	a = UNSET;
	b = UNSET;
	c = UNSET;
	// The uncached cases write past the cache, so it must not hold these.
	sceKernelDcacheWritebackInvalidateAll();
}

static const char *val(u32 v) {
	static char buf[4][16];
	static int i = 0;
	i = (i + 1) & 3;
	if (v == UNSET) {
		return "untouched";
	}
	snprintf(buf[i], sizeof(buf[i]), "%08x", (unsigned)v);
	return buf[i];
}

template <typename T>
static T *kernelPtr(T *p) {
	return (T *)((u32)p | 0x80000000);
}

template <typename T>
static T *uncachedPtr(T *p) {
	return (T *)((u32)p | 0x40000000);
}

static void testGetFrameBuf(const char *title, void **top, int *stride, int *fmt) {
	reset();
	int result = sceDisplayGetFrameBuf(top, stride, fmt, PSP_DISPLAY_SETBUF_IMMEDIATE);
	printf("  %s: %08x, top=%s stride=%s fmt=%s\n", title, result, val(a), val(b), val(c));
}

static void testGetMode(const char *title, int *mode, int *width, int *height) {
	reset();
	int result = sceDisplayGetMode(mode, width, height);
	printf("  %s: %08x, mode=%s width=%s height=%s\n", title, result, val(a), val(b), val(c));
}

static void testGetBrightness(const char *title, int *level, int *unk) {
	reset();
	int result = sceDisplayGetBrightness(level, unk);
	// The level is whatever the PSP is set to.
	printf("  %s: %08x, level %s, unk=%s\n", title, result, a == UNSET ? "untouched" : "written", val(b));
}

static void testSetFrameBuf(u32 addr) {
	// Put back before the next vblank, so the display never shows it.
	sceDisplayWaitVblankStart();
	int result = sceDisplaySetFrameBuf((void *)addr, 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
	sceDisplaySetFrameBuf(vram, 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
	printf("  %08x: %08x\n", (unsigned)addr, result);
}

extern "C" int main(int argc, char *argv[]) {
	HAS_DISPLAY = 0;

	sceDisplaySetMode(0, 480, 272);
	sceDisplaySetFrameBuf(vram, 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
	sceDisplaySetFrameBuf(vram, 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
	sceDisplayWaitVblankStart();

	void **pa = (void **)&a;
	int *pb = (int *)&b;
	int *pc = (int *)&c;

	printf("sceDisplayGetFrameBuf:\n");
	testGetFrameBuf("Normal", pa, pb, pc);
	testGetFrameBuf("NULL top", NULL, pb, pc);
	testGetFrameBuf("NULL stride and format", pa, NULL, NULL);
	testGetFrameBuf("Kernel top", kernelPtr(pa), pb, pc);
	testGetFrameBuf("Kernel stride", pa, kernelPtr(pb), pc);
	testGetFrameBuf("Kernel format", pa, pb, kernelPtr(pc));
	testGetFrameBuf("Uncached", uncachedPtr(pa), uncachedPtr(pb), uncachedPtr(pc));

	printf("sceDisplayGetMode:\n");
	testGetMode("Normal", (int *)pa, pb, pc);
	testGetMode("NULL", NULL, NULL, NULL);
	testGetMode("Kernel mode", kernelPtr((int *)pa), pb, pc);
	testGetMode("Kernel height", (int *)pa, pb, kernelPtr(pc));
	testGetMode("Uncached", uncachedPtr((int *)pa), uncachedPtr(pb), uncachedPtr(pc));

	printf("sceDisplayGetBrightness:\n");
	testGetBrightness("Normal", (int *)pa, pb);
	testGetBrightness("Kernel level", kernelPtr((int *)pa), pb);
	testGetBrightness("Kernel unk", (int *)pa, kernelPtr(pb));

	printf("sceDisplayGetResumeMode:\n");
	reset();
	int result = sceDisplayGetResumeMode((int *)pa);
	printf("  Normal: %08x, mode=%s\n", result, val(a));
	reset();
	result = sceDisplayGetResumeMode(kernelPtr((int *)pa));
	printf("  Kernel: %08x, mode=%s\n", result, val(a));

	printf("sceDisplaySetFrameBuf addresses:\n");
	static const u32 addrs[] = {
		0x04000000, 0x44000000, 0x84000000, 0xA4000000, 0xC4000000, 0x24000000, 0x14000000,
		0x047FFFF0, 0x04800000, 0x04600000, 0x05000000,
		0x08000000, 0x48800000, 0x88800000, 0xA8800000, 0xC8800000, 0x28800000, 0x18800000,
		0x09FFFFF0, 0x0A000000, 0x0BFFFFF0, 0x0C000000, 0x0E000000, 0x0FFFFFF0, 0x10000000,
	};
	for (size_t i = 0; i < ARRAY_SIZE(addrs); ++i) {
		testSetFrameBuf(addrs[i]);
	}

	reset();
	sceDisplayGetFrameBuf(pa, pb, pc, PSP_DISPLAY_SETBUF_NEXTFRAME);
	printf("Latched after: top=%s\n", val(a));

	return 0;
}
