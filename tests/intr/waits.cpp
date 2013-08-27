#include <common.h>

#include <pspthreadman.h>
#include <pspintrman.h>
#include <pspdisplay.h>
#include <stdarg.h>

extern SceUID reschedThread;
extern volatile int didResched;

extern u64 lastCheckpoint;
void safeCheckpoint(const char *format, ...) {
	int state = sceKernelSuspendDispatchThread();
	sceKernelResumeDispatchThread(state);

	u64 currentCheckpoint = sceKernelGetSystemTimeWide();
	if (CHECKPOINT_ENABLE_TIME) {
		schedf("[%s/%lld] ", didResched ? "r" : "x", currentCheckpoint - lastCheckpoint);
	} else {
		schedf("[%s] ", didResched ? "r" : "x");
	}

	if (state == 1) {
		sceKernelTerminateThread(reschedThread);
	}

	if (format != NULL) {
		va_list args;
		va_start(args, format);
		schedfBufferPos += vsprintf(schedfBuffer + schedfBufferPos, format, args);
		// This is easier to debug in the emulator, but printf() reschedules on the real PSP.
		//vprintf(format, args);
		va_end(args);
	}

	if (state == 1) {
		didResched = 0;
		sceKernelStartThread(reschedThread, 0, NULL);
	}

	if (format != NULL) {
		schedf("\n");
	}

	lastCheckpoint = currentCheckpoint;
}

#define INTR_DISPATCH(expr) \
	{ \
		state = sceKernelCpuSuspendIntr(); \
		safeCheckpoint("  Interrupts disabled: %08x", expr); \
		sceKernelCpuResumeIntr(state); \
		state = sceKernelSuspendDispatchThread(); \
		safeCheckpoint("  Dispatch disabled: %08x", expr); \
		sceKernelResumeDispatchThread(state); \
	}

#define INTR_DISPATCH_TITLE(title, expr) \
	{ \
		state = sceKernelCpuSuspendIntr(); \
		safeCheckpoint("  %s - interrupts disabled: %08x", title, expr); \
		sceKernelCpuResumeIntr(state); \
		state = sceKernelSuspendDispatchThread(); \
		safeCheckpoint("  %s - dispatch disabled: %08x", title, expr); \
		sceKernelResumeDispatchThread(state); \
	}

extern "C" int main(int argc, char *argv[]) {
	int state;

	checkpointNext("sceKernelDelayThread:");
	INTR_DISPATCH(sceKernelDelayThread(200));
	checkpointNext("sceKernelDelayThreadCB:");
	INTR_DISPATCH(sceKernelDelayThreadCB(200));
	checkpointNext("sceKernelSleepThread:");
	INTR_DISPATCH(sceKernelSleepThread());
	checkpointNext("sceKernelSleepThreadCB:");
	INTR_DISPATCH(sceKernelSleepThreadCB());

	checkpointNext("sceDisplayWaitVblank:");
	INTR_DISPATCH(sceDisplayWaitVblank());
	checkpointNext("sceDisplayWaitVblankCB:");
	INTR_DISPATCH(sceDisplayWaitVblankCB());
	checkpointNext("sceDisplayWaitVblankStart:");
	INTR_DISPATCH(sceDisplayWaitVblankStart());
	checkpointNext("sceDisplayWaitVblankStartCB:");
	INTR_DISPATCH(sceDisplayWaitVblankStartCB());

	SceUID sema = sceKernelCreateSema("sema", 0, 0, 1, NULL);
	checkpointNext("sceKernelWaitSema:");
	INTR_DISPATCH_TITLE("Bad sema", sceKernelWaitSema(0, 1, NULL));
	INTR_DISPATCH_TITLE("Invalid count", sceKernelWaitSema(sema, 9, NULL));
	INTR_DISPATCH_TITLE("Valid sema", sceKernelWaitSema(sema, 1, NULL));
	checkpointNext("sceKernelWaitSemaCB:");
	INTR_DISPATCH_TITLE("Bad sema", sceKernelWaitSemaCB(0, 1, NULL));
	INTR_DISPATCH_TITLE("Invalid count", sceKernelWaitSemaCB(sema, 9, NULL));
	INTR_DISPATCH_TITLE("Valid sema", sceKernelWaitSemaCB(sema, 1, NULL));
	sceKernelDeleteSema(sema);

	SceUID flag = sceKernelCreateEventFlag("flag", 0, 0, NULL);
	checkpointNext("sceKernelWaitEventFlag:");
	INTR_DISPATCH_TITLE("Bad flag", sceKernelWaitEventFlag(0, 1, PSP_EVENT_WAITAND, NULL, NULL));
	INTR_DISPATCH_TITLE("Invalid mode", sceKernelWaitEventFlag(flag, 1, 0xFF, NULL, NULL));
	INTR_DISPATCH_TITLE("Valid sema", sceKernelWaitEventFlag(flag, 1, PSP_EVENT_WAITAND, NULL, NULL));
	sceKernelSetEventFlag(flag, 1);
	INTR_DISPATCH_TITLE("Already set", sceKernelWaitEventFlag(flag, 1, PSP_EVENT_WAITAND, NULL, NULL));
	sceKernelDeleteEventFlag(flag);
	flag = sceKernelCreateEventFlag("flag", 0, 0, NULL);
	checkpointNext("sceKernelWaitEventFlagCB:");
	INTR_DISPATCH_TITLE("Bad flag", sceKernelWaitEventFlagCB(0, 1, PSP_EVENT_WAITAND, NULL, NULL));
	INTR_DISPATCH_TITLE("Invalid mode", sceKernelWaitEventFlagCB(flag, 1, 0xFF, NULL, NULL));
	INTR_DISPATCH_TITLE("Valid sema", sceKernelWaitEventFlagCB(flag, 1, PSP_EVENT_WAITAND, NULL, NULL));
	sceKernelSetEventFlag(flag, 1);
	INTR_DISPATCH_TITLE("Already set", sceKernelWaitEventFlagCB(flag, 1, PSP_EVENT_WAITAND, NULL, NULL));
	sceKernelDeleteEventFlag(flag);

	return 0;
}