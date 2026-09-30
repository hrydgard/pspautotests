#include "shared.h"

// What a caller sees right after sceUtilitySavedataInitStart, and on the way to RUNNING, by its
// priority against the dialog threads (graphics 17, access 19, font 18, sound 16, as Freak Out and
// NFL Street 3 set them). SIZES on a save that doesn't exist. Polls yield, so the dialog threads
// always get to run.

static SceUtilitySavedataSizeInfo sizeInfo;

static const char *statusName(int s) {
	switch (s) {
	case 0: return "NONE";
	case 1: return "INIT";
	case 2: return "RUNNING";
	case 3: return "FINISHED";
	case 4: return "SHUTDOWN";
	default: return "?";
	}
}

static void finish() {
	for (int frame = 0; frame < 600; ++frame) {
		int s = sceUtilitySavedataGetStatus();
		if (s == 3 || s < 0) {
			break;
		}
		if (s == 2) {
			sceUtilitySavedataUpdate(1);
		}
		sceDisplayWaitVblankStart();
	}
	sceUtilitySavedataShutdownStart();
	for (int frame = 0; frame < 600 && sceUtilitySavedataGetStatus() != 0; ++frame) {
		sceDisplayWaitVblankStart();
	}
}

static void runCase(int caller) {
	sceKernelChangeThreadPriority(0, caller);

	SceUtilitySavedataParam2 param;
	initStandardSavedataParams(&param);
	strcpy(param.gameName, "TEST99909");
	param.mode = (PspUtilitySavedataMode)8;
	param.sizeInfo = &sizeInfo;
	param.base.graphicsThread = 17;
	param.base.accessThread = 19;
	param.base.fontThread = 18;
	param.base.soundThread = 16;
	int result = sceUtilitySavedataInitStart((SceUtilitySavedataParam *)&param);
	int status = sceUtilitySavedataGetStatus();

	char seen[128] = "";
	int last = status;
	u32 start = sceKernelGetSystemTimeLow();
	while (sceKernelGetSystemTimeLow() - start < 2000000) {
		sceKernelDelayThread(100);
		int s = sceUtilitySavedataGetStatus();
		if (s != last && strlen(seen) < sizeof(seen) - 16) {
			strcat(seen, " ");
			strcat(seen, statusName(s));
			last = s;
		}
		if (s >= 2 || s < 0) {
			break;
		}
	}
	printf("caller %d: InitStart %08x, then %s, then%s\n", caller, result, statusName(status), seen);
	finish();
	sceKernelChangeThreadPriority(0, 0x20);
	sceKernelDelayThread(1000);
}

int main(int argc, char **argv) {
	initDisplay();
	static const int callers[] = { 16, 18, 20, 32, 33, 48, 111 };
	for (int i = 0; i < (int)(sizeof(callers) / sizeof(callers[0])); ++i) {
		runCase(callers[i]);
	}
	return 0;
}
