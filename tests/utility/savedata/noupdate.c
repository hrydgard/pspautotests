#include "shared.h"

// Savedata without UI (SIZES) driven by GetStatus alone: does it get to FINISHED without any
// Update calls? Afterwards Update is called anyway, so the dialog always gets back to NONE.

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
	if (result < 0) {
		printf("caller %d: InitStart %08x\n", caller, result);
		sceKernelChangeThreadPriority(0, 0x20);
		return;
	}

	// GetStatus only, for up to two seconds.
	char seen[128] = "";
	int last = -99;
	u32 start = sceKernelGetSystemTimeLow();
	while (sceKernelGetSystemTimeLow() - start < 2000000) {
		int s = sceUtilitySavedataGetStatus();
		if (s != last && strlen(seen) < sizeof(seen) - 16) {
			strcat(seen, " ");
			strcat(seen, statusName(s));
			last = s;
		}
		if (s == 3 || s < 0) {
			break;
		}
		sceKernelDelayThread(1000);
	}
	printf("caller %d, no Update:%s\n", caller, seen);

	// Finish it properly either way.
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
	printf("  then with Update: %s, result %08x\n", statusName(sceUtilitySavedataGetStatus()), param.base.result);
	sceUtilitySavedataShutdownStart();
	for (int frame = 0; frame < 600 && sceUtilitySavedataGetStatus() != 0; ++frame) {
		sceDisplayWaitVblankStart();
	}
	sceKernelChangeThreadPriority(0, 0x20);
	sceKernelDelayThread(1000);
}

int main(int argc, char **argv) {
	initDisplay();
	runCase(0x20);
	runCase(0x6F);
	return 0;
}
