#include <common.h>
#include <pspgu.h>
#include <psprtc.h>
#include <pspdisplay.h>

int sceDisplayWaitVblankStartMulti(int vblanks);
int sceDisplayWaitVblankStartMultiCB(int vblanks);
int sceDisplayIsVblank();
int sceDisplayIsVsync();

static char schedulingLog[65536];
static volatile int schedulingLogPos = 0;

inline void schedf(const char *format, ...) {
	va_list args;
	va_start(args, format);
	schedulingLogPos += vsprintf(schedulingLog + schedulingLogPos, format, args);
	// This is easier to debug in the emulator, but printf() reschedules on the real PSP.
	//vprintf(format, args);
	va_end(args);
}

inline void flushschedf() {
	printf("%s", schedulingLog);
	schedulingLogPos = 0;
	schedulingLog[0] = '\0';
}

void testWait(const char *title, int (*wait)()) {
	// Wait until just a bit after a vblank.
	sceDisplayWaitVblankStart();
	sceKernelDelayThread(750);
	int vbase = sceDisplayGetVcount();

	schedf("%s:\n", title);
	int i;
	for (i = 0; i < 4; ++i) {
		schedf("i=%-4d vcount=%-4d ", i, sceDisplayGetVcount() - vbase);

		int result = wait();
		schedf("wait=%08x  vblank=%d\n", result, sceDisplayIsVblank());
	}
	flushschedf();
}

void testWaitMulti(const char *title, int (*multi)(int)) {
	// Wait until just a bit after a vblank.
	sceDisplayWaitVblankStart();
	sceKernelDelayThread(750);
	int vbase = sceDisplayGetVcount();

	schedf("%s:\n", title);
	int i;
	for (i = 0; i < 4; ++i) {
		schedf("i=%-4d vcount=%-4d ", i, sceDisplayGetVcount() - vbase);

		int result = multi(3);
		schedf("wait=%08x  vblank=%d\n", result, sceDisplayIsVblank());
	}
	flushschedf();
}

int main(int argc, char *argv[]) {
	testWait("Start", &sceDisplayWaitVblankStart);
	testWait("StartCB", &sceDisplayWaitVblankStartCB);
	testWait("Vblank", &sceDisplayWaitVblank);
	testWait("VblankCB", &sceDisplayWaitVblankCB);
	testWaitMulti("Multi", &sceDisplayWaitVblankStartMultiCB);
	testWaitMulti("MultiCB", &sceDisplayWaitVblankStartMulti);

	return 0;
}