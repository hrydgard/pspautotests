#include <common.h>
#include <pspdisplay.h>
#include <pspthreadman.h>

// Which vblank sceDisplayWaitVblankStartMulti(n) returns at, when the caller's frame took longer than
// a vblank: n from the call, or n from where the thread's previous vblank wait ended.

extern "C" {
int sceDisplayWaitVblankStartMulti(int vblanks);
int sceDisplayWaitVblankStartMultiCB(int vblanks);
int sceDisplayIsVblank();
}

extern "C" int HAS_DISPLAY;

typedef int (*MultiFunc)(int);

static const int FRAME_US = 16683;

static void busyUntil(u32 start, int us) {
	while ((int)(sceKernelGetSystemTimeLow() - start) < us) {
		continue;
	}
}

// Syncs to a vblank with sceDisplayWaitVblankStart, lets delayUs pass, then waits for n.
static void testAfterStart(const char *title, MultiFunc multi, int delayUs, int n) {
	sceDisplayWaitVblankStart();
	u32 start = sceKernelGetSystemTimeLow();
	int base = sceDisplayGetVcount();
	busyUntil(start, delayUs);
	int called = sceDisplayGetVcount() - base;
	int result = multi(n);
	int returned = sceDisplayGetVcount() - base;
	printf("  %s: n=%d, called at +%d, %08x at +%d\n", title, n, called, result, returned);
}

static void testMulti(const char *title, MultiFunc multi) {
	printf("%s\n", title);
	testAfterStart("0.3 frames", multi, 5000, 2);
	testAfterStart("1.3 frames", multi, 21700, 1);
	testAfterStart("1.3 frames", multi, 21700, 2);
	testAfterStart("1.3 frames", multi, 21700, 3);
	testAfterStart("2.3 frames", multi, 38400, 2);
	testAfterStart("2.3 frames", multi, 38400, 3);
	// About 40us before the next vblank, too late to catch it.
	testAfterStart("Just before a vblank", multi, FRAME_US - 120, 2);
	testAfterStart("Just before a vblank", multi, FRAME_US - 120, 3);
	// Well past where it started counting: one vblank, or n?
	testAfterStart("10.3 frames", multi, 10 * FRAME_US + 5000, 3);
}

// Whether a sceDisplayWaitVblank, rather than ...Start, sets where the next Multi counts from.
static void testAfterWaitVblank() {
	printf("After sceDisplayWaitVblank:\n");

	// Outside a vblank, it waits for the next one.
	sceDisplayWaitVblankStart();
	u32 start = sceKernelGetSystemTimeLow();
	busyUntil(start, 2000);
	int result = sceDisplayWaitVblank();
	start = sceKernelGetSystemTimeLow();
	int base = sceDisplayGetVcount();
	busyUntil(start, 21700);
	int called = sceDisplayGetVcount() - base;
	int multi = sceDisplayWaitVblankStartMulti(3);
	int returned = sceDisplayGetVcount() - base;
	printf("  Waited: %08x, Multi(3) called at +%d, %08x at +%d\n", result, called, multi, returned);

	// Inside a vblank it returns 1 at once. This one is two vblanks after the last real wait.
	sceDisplayWaitVblankStart();
	start = sceKernelGetSystemTimeLow();
	base = sceDisplayGetVcount();
	busyUntil(start, 2 * FRAME_US + 200);
	int inVblank = sceDisplayIsVblank();
	int at = sceDisplayGetVcount() - base;
	result = sceDisplayWaitVblank();
	busyUntil(start, 2 * FRAME_US + 21700);
	called = sceDisplayGetVcount() - base;
	multi = sceDisplayWaitVblankStartMulti(3);
	returned = sceDisplayGetVcount() - base;
	printf("  In vblank=%d at +%d: %08x, Multi(3) called at +%d, %08x at +%d\n", inVblank, at, result, called, multi, returned);
}

static int freshThread(SceSize argc, void *argv) {
	// Sync to a vblank without a vblank wait, since that's what's being tested.
	int base = sceDisplayGetVcount();
	while ((int)sceDisplayGetVcount() == base) {
		continue;
	}
	base = sceDisplayGetVcount();
	u32 start = sceKernelGetSystemTimeLow();
	busyUntil(start, 5000);
	int result = sceDisplayWaitVblankStartMulti(3);
	int returned = sceDisplayGetVcount() - base;
	printf("  First wait: Multi(3) from +0, %08x at +%d\n", result, returned);

	start = sceKernelGetSystemTimeLow();
	base = sceDisplayGetVcount();
	busyUntil(start, 21700);
	int called = sceDisplayGetVcount() - base;
	result = sceDisplayWaitVblankStartMulti(3);
	returned = sceDisplayGetVcount() - base;
	printf("  Second wait: Multi(3) called at +%d, %08x at +%d\n", called, result, returned);
	return 0;
}

static void testFreshThread() {
	printf("On a new thread:\n");
	SceUID thread = sceKernelCreateThread("fresh", &freshThread, 0x20, 0x1000, 0, NULL);
	sceKernelStartThread(thread, 0, NULL);
	SceUInt timeout = 2000000;
	int result = sceKernelWaitThreadEnd(thread, &timeout);
	if (result != 0) {
		printf("  Thread did not finish: %08x\n", result);
		sceKernelTerminateThread(thread);
	}
	sceKernelDeleteThread(thread);
}

extern "C" int main(int argc, char *argv[]) {
	HAS_DISPLAY = 0;

	testMulti("sceDisplayWaitVblankStartMulti:", &sceDisplayWaitVblankStartMulti);
	testMulti("sceDisplayWaitVblankStartMultiCB:", &sceDisplayWaitVblankStartMultiCB);
	testAfterWaitVblank();
	testFreshThread();

	return 0;
}
