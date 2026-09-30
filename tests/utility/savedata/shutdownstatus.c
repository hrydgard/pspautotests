#include "shared.h"

// What a game with worse priority than the dialog threads sees after ShutdownStart, for SIZES on a
// save that exists and on one that doesn't. The dialog threads are at 17 and 19, and the caller
// polls with a short delay between reads, so they always get to run.
//
// Freak Out (caller 32, a save that exists) waits for SHUTDOWN after ShutdownStart; NFL Street 3
// (caller 111) calls InitStart again straight after it.

static SceUtilitySavedataSizeInfo sizeInfo;
static char savedata[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6 };

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

// Runs a started savedata to FINISHED, waiting a vblank between polls.
static int runToFinished() {
	for (int frame = 0; frame < 600; ++frame) {
		int status = sceUtilitySavedataGetStatus();
		if (status == 3 || status < 0) {
			return status;
		}
		if (status == 2) {
			sceUtilitySavedataUpdate(1);
		}
		sceDisplayWaitVblankStart();
	}
	return -1;
}

static void waitForNone() {
	for (int frame = 0; frame < 600 && sceUtilitySavedataGetStatus() != 0; ++frame) {
		sceDisplayWaitVblankStart();
	}
}

static void runCase(const char *title, int caller, const char *gameName, int graphics, int access, int font, int sound) {
	sceKernelChangeThreadPriority(0, caller);

	SceUtilitySavedataParam2 param;
	initStandardSavedataParams(&param);
	strcpy(param.gameName, gameName);
	param.mode = (PspUtilitySavedataMode)8;
	param.sizeInfo = &sizeInfo;
	param.base.graphicsThread = graphics;
	param.base.accessThread = access;
	param.base.fontThread = font;
	param.base.soundThread = sound;
	int result = sceUtilitySavedataInitStart((SceUtilitySavedataParam *)&param);
	if (result < 0) {
		printf("%s, caller %d: InitStart %08x\n", title, caller, result);
		sceKernelChangeThreadPriority(0, 0x20);
		return;
	}
	runToFinished();

	result = sceUtilitySavedataShutdownStart();
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
		if (s == 0) {
			break;
		}
		sceKernelDelayThread(100);
	}
	printf("%s, caller %d, threads %d/%d/%d/%d: ShutdownStart %08x, then%s\n", title, caller, graphics, access, font, sound, result, seen);
	waitForNone();
	sceKernelChangeThreadPriority(0, 0x20);
	sceKernelDelayThread(1000);
}

int main(int argc, char **argv) {
	initDisplay();

	// Make a save to size.
	SceUtilitySavedataParam2 make;
	initStandardSavedataParams(&make);
	make.mode = (PspUtilitySavedataMode)14;  // MAKEDATA
	make.dataBuf = savedata;
	make.dataBufSize = sizeof(savedata);
	make.dataSize = sizeof(savedata);
	int result = sceUtilitySavedataInitStart((SceUtilitySavedataParam *)&make);
	if (result == 0 && runToFinished() == 3) {
		sceUtilitySavedataShutdownStart();
		waitForNone();
	}
	printf("MAKEDATA: %08x, result %08x\n", result, make.base.result);

	// As the games set them: graphics 17, access 19, font 18, sound 16.
	runCase("no save", 32, "TEST99909", 17, 19, 18, 16);
	runCase("no save", 111, "TEST99909", 17, 19, 18, 16);
	runCase("existing save", 32, "TEST99901", 17, 19, 18, 16);
	runCase("existing save", 111, "TEST99901", 17, 19, 18, 16);
	// Which of font and sound the earlier threshold was about.
	static const int callers[] = { 33, 34, 35, 36 };
	for (int i = 0; i < 4; ++i) {
		runCase("no save", callers[i], "TEST99909", 17, 19, 0x23, 0x20);
	}
	runCase("no save", 33, "TEST99909", 17, 19, 0x40, 0x20);
	runCase("no save", 33, "TEST99909", 17, 19, 0x23, 0x40);

	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901ABC/DATA.BIN");
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901ABC/PARAM.SFO");
	sceIoRmdir("ms0:/PSP/SAVEDATA/TEST99901ABC");
	return 0;
}
