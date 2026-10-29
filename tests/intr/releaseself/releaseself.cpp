#include <common.h>
#include <pspintrman.h>
#include <pspdisplay.h>

// What happens when a VBLANK subinterrupt handler releases itself, or another handler due in the
// same VBLANK, from inside the interrupt. Nothing is printed from interrupt context: the handlers
// log into an array that main() prints afterwards.

enum {
	SUB_SELF = 1,
	SUB_OTHER = 2,
	SUB_VICTIM = 3,
};

static volatile int logEntries[64];
static volatile int logCount = 0;
static volatile int selfResult = 0x12345678;
static volatile int otherResult = 0x12345678;
static volatile int selfCalls = 0;
static volatile int otherCalls = 0;
static volatile int victimCalls = 0;

static void logEntry(int v) {
	if (logCount < (int)ARRAY_SIZE(logEntries)) {
		logEntries[logCount++] = v;
	}
}

static int selfHandler(int sub, void *arg) {
	logEntry(sub);
	if (selfCalls++ == 0) {
		selfResult = sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, SUB_SELF);
	}
	return -1;
}

static int otherHandler(int sub, void *arg) {
	logEntry(sub);
	if (otherCalls++ == 0) {
		otherResult = sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, SUB_VICTIM);
	}
	return -1;
}

static int victimHandler(int sub, void *arg) {
	logEntry(sub);
	victimCalls++;
	return -1;
}

static void printLog() {
	char buf[256] = {0};
	int pos = 0;
	for (int i = 0; i < logCount && pos < (int)sizeof(buf) - 8; ++i) {
		pos += snprintf(buf + pos, sizeof(buf) - pos, " %d", logEntries[i]);
	}
	checkpoint("  Call order:%s", buf);
}

static void resetLog() {
	logCount = 0;
	selfResult = 0x12345678;
	otherResult = 0x12345678;
	selfCalls = 0;
	otherCalls = 0;
	victimCalls = 0;
}

static void waitVblanks(int n) {
	for (int i = 0; i < n; ++i) {
		sceDisplayWaitVblankStart();
	}
}

extern "C" int main(int argc, char *argv[]) {
	// In case an earlier run left something behind.
	sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, SUB_SELF);
	sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, SUB_OTHER);
	sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, SUB_VICTIM);

	checkpointNext("Handler releasing itself:");
	resetLog();
	checkpoint("  Register: %08x", sceKernelRegisterSubIntrHandler(PSP_VBLANK_INT, SUB_SELF, (void *)&selfHandler, NULL));
	checkpoint("  Enable: %08x", sceKernelEnableSubIntr(PSP_VBLANK_INT, SUB_SELF));
	waitVblanks(4);
	checkpoint("  Release in handler: %08x", selfResult);
	checkpoint("  Calls: %d", selfCalls);
	printLog();
	checkpoint("  Release again: %08x", sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, SUB_SELF));

	checkpointNext("Handler releasing a later one in the same VBLANK:");
	resetLog();
	checkpoint("  Register other: %08x", sceKernelRegisterSubIntrHandler(PSP_VBLANK_INT, SUB_OTHER, (void *)&otherHandler, NULL));
	checkpoint("  Register victim: %08x", sceKernelRegisterSubIntrHandler(PSP_VBLANK_INT, SUB_VICTIM, (void *)&victimHandler, NULL));
	// Enable them with interrupts off, so both are due from the same VBLANK.
	int flags = sceKernelCpuSuspendIntr();
	sceKernelEnableSubIntr(PSP_VBLANK_INT, SUB_OTHER);
	sceKernelEnableSubIntr(PSP_VBLANK_INT, SUB_VICTIM);
	sceKernelCpuResumeIntr(flags);
	waitVblanks(4);
	checkpoint("  Release in handler: %08x", otherResult);
	checkpoint("  Other calls: %d", otherCalls);
	checkpoint("  Victim calls: %d", victimCalls);
	printLog();
	checkpoint("  Release victim again: %08x", sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, SUB_VICTIM));
	checkpoint("  Release other: %08x", sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, SUB_OTHER));

	return 0;
}
