#include <common.h>
#include <pspintrman.h>
#include <pspdisplay.h>

// VBLANK subinterrupt handlers due in the same VBLANK, where the first disables the second, or
// re-registers itself, from inside the interrupt. Nothing is printed from interrupt context.

static volatile int logEntries[64];
static volatile int logCount = 0;
static volatile int firstResult = 0x12345678;
static volatile int mode = 0;

static void logEntry(int v) {
	if (logCount < (int)ARRAY_SIZE(logEntries)) {
		logEntries[logCount++] = v;
	}
}

static int firstHandler(int sub, void *arg) {
	logEntry(sub);
	if (logCount == 1) {
		if (mode == 0) {
			firstResult = sceKernelDisableSubIntr(PSP_VBLANK_INT, 2);
		} else if (mode == 1) {
			firstResult = sceKernelEnableSubIntr(PSP_VBLANK_INT, 3);
		}
	}
	return -1;
}

static int secondHandler(int sub, void *arg) {
	logEntry(sub);
	return -1;
}

static void printLog(const char *title) {
	char buf[256] = {0};
	int pos = 0;
	for (int i = 0; i < logCount && pos < (int)sizeof(buf) - 8; ++i) {
		pos += snprintf(buf + pos, sizeof(buf) - pos, " %d", logEntries[i]);
	}
	checkpoint("  %s: %08x,%s", title, firstResult, buf);
	logCount = 0;
	firstResult = 0x12345678;
}

static void releaseAll() {
	for (int i = 1; i <= 3; ++i) {
		sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, i);
	}
}

extern "C" int main(int argc, char *argv[]) {
	releaseAll();

	checkpointNext("First disables the second:");
	mode = 0;
	sceKernelRegisterSubIntrHandler(PSP_VBLANK_INT, 1, (void *)&firstHandler, NULL);
	sceKernelRegisterSubIntrHandler(PSP_VBLANK_INT, 2, (void *)&secondHandler, NULL);
	int flags = sceKernelCpuSuspendIntr();
	sceKernelEnableSubIntr(PSP_VBLANK_INT, 1);
	sceKernelEnableSubIntr(PSP_VBLANK_INT, 2);
	sceKernelCpuResumeIntr(flags);
	sceDisplayWaitVblankStart();
	sceDisplayWaitVblankStart();
	sceKernelDisableSubIntr(PSP_VBLANK_INT, 1);
	printLog("Calls");
	releaseAll();

	checkpointNext("First enables a third:");
	mode = 1;
	sceKernelRegisterSubIntrHandler(PSP_VBLANK_INT, 1, (void *)&firstHandler, NULL);
	sceKernelRegisterSubIntrHandler(PSP_VBLANK_INT, 3, (void *)&secondHandler, NULL);
	sceKernelEnableSubIntr(PSP_VBLANK_INT, 1);
	sceDisplayWaitVblankStart();
	sceDisplayWaitVblankStart();
	sceKernelDisableSubIntr(PSP_VBLANK_INT, 1);
	sceKernelDisableSubIntr(PSP_VBLANK_INT, 3);
	printLog("Calls");
	releaseAll();

	return 0;
}
