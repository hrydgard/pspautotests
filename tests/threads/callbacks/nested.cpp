#include "shared.h"

// A callback that itself processes callbacks, directly or through a CB wait.

static SceUID cbOuter;
static SceUID cbInner;
static SceUID flag;
static SceUID freeSema;
static int mode;

static int innerFunc(int arg1, int arg2, void *arg) {
	checkpoint("    * innerFunc hit: %08x, %08x", arg1, arg2);
	return 0;
}

static int outerFunc(int arg1, int arg2, void *arg) {
	checkpoint("  * outerFunc hit: %08x, %08x", arg1, arg2);
	sceKernelNotifyCallback(cbInner, arg2);
	switch (mode) {
	case 0:
		checkpoint("  sceKernelCheckCallback in callback: %08x", sceKernelCheckCallback());
		break;
	case 1:
		checkpoint("  sceKernelWaitSemaCB (available) in callback: %08x", sceKernelWaitSemaCB(freeSema, 1, NULL));
		sceKernelSignalSema(freeSema, 1);
		break;
	case 2:
		checkpoint("  sceKernelDelayThreadCB in callback: %08x", sceKernelDelayThreadCB(500));
		break;
	case 3: {
		SceUInt timeout = 500;
		checkpoint("  sceKernelWaitEventFlagCB (timeout) in callback: %08x", sceKernelWaitEventFlagCB(flag, 1, PSP_EVENT_WAITOR, NULL, &timeout));
		break;
	}
	}
	checkpoint("  sceKernelCheckCallback after, in callback: %08x", sceKernelCheckCallback());
	return 0;
}

static void runOuter(const char *title) {
	checkpointNext(title);
	for (mode = 0; mode < 4; ++mode) {
		sceKernelNotifyCallback(cbOuter, mode);
		checkpoint("  sceKernelCheckCallback: %08x", sceKernelCheckCallback());
	}
}

static int setterFunc(SceSize argc, void *argv) {
	sceKernelDelayThread(1000);
	sceKernelSetEventFlag(flag, 2);
	return 0;
}

extern "C" int main(int argc, char *argv[]) {
	cbOuter = sceKernelCreateCallback("outer", &outerFunc, NULL);
	cbInner = sceKernelCreateCallback("inner", &innerFunc, NULL);
	flag = sceKernelCreateEventFlag("flag", 0, 0, NULL);
	freeSema = sceKernelCreateSema("free", 0, 1, 1, NULL);

	runOuter("From sceKernelCheckCallback:");

	checkpointNext("From a blocking sceKernelWaitEventFlagCB:");
	for (mode = 0; mode < 4; ++mode) {
		SceUID setter = sceKernelCreateThread("setter", &setterFunc, 0x10, 0x1000, 0, NULL);
		sceKernelStartThread(setter, 0, NULL);
		sceKernelNotifyCallback(cbOuter, mode);
		u32 bits = 0;
		checkpoint("  sceKernelWaitEventFlagCB: %08x", sceKernelWaitEventFlagCB(flag, 2, PSP_EVENT_WAITOR | PSP_EVENT_WAITCLEAR, &bits, NULL));
		checkpoint("  bits: %08x", bits);
		sceKernelDelayThread(2000);
		sceKernelDeleteThread(setter);
	}

	sceKernelDeleteSema(freeSema);
	sceKernelDeleteEventFlag(flag);
	sceKernelDeleteCallback(cbInner);
	sceKernelDeleteCallback(cbOuter);
	return 0;
}
