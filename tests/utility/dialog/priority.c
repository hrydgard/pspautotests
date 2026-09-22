#include "shared.h"
#include <string.h>

// What a caller sees right after InitStart and ShutdownStart, depending on how its priority
// compares to the dialog's graphicsThread and accessThread. Also whether a new InitStart straight
// after ShutdownStart gets through (NFL Street 3 does that, from priority 111).

static void runCase(int caller, int graphics, int access) {
	printf("caller=%d graphics=%d access=%d:\n", caller, graphics, access);
	sceKernelChangeThreadPriority(0, caller);

	MsgDialogParams msg;
	initMsgParams(&msg, graphics, access);
	int result = sceUtilityMsgDialogInitStart((pspUtilityMsgDialogParams *)&msg);
	int status = sceUtilityMsgDialogGetStatus();
	printf("  InitStart: %s, then %s\n", statusName(result), statusName(status));

	int frame = 0;
	while (sceUtilityMsgDialogGetStatus() == 1 && frame++ < 600) {
		sceDisplayWaitVblankStart();
	}
	sceUtilityMsgDialogAbort();
	while (sceUtilityMsgDialogGetStatus() == 2 && frame++ < 600) {
		sceUtilityMsgDialogUpdate(1);
		sceDisplayWaitVblankStart();
	}

	result = sceUtilityMsgDialogShutdownStart();
	status = sceUtilityMsgDialogGetStatus();
	printf("  ShutdownStart: %s, then %s\n", statusName(result), statusName(status));

	SavedataParams save;
	initSavedataParams(&save);
	result = sceUtilitySavedataInitStart((SceUtilitySavedataParam *)&save);
	printf("  savedata InitStart right after: %s\n", statusName(result));
	if (result >= 0) {
		printf("  savedata: %s\n", finishSavedata() >= 0 ? "ok" : "failed");
	}

	while (sceUtilityMsgDialogGetStatus() == 4 && frame++ < 600) {
		sceDisplayWaitVblankStart();
	}
	sceKernelChangeThreadPriority(0, 0x20);
	sceKernelDelayThread(1000);
}

int main(int argc, char *argv[]) {
	initDisplay();

	// Better than both, between, and worse than both.
	runCase(0x10, 0x11, 0x13);
	runCase(0x12, 0x11, 0x13);
	runCase(0x6F, 0x11, 0x13);
	// And with graphics and access the other way around.
	runCase(0x10, 0x13, 0x11);
	runCase(0x12, 0x13, 0x11);
	runCase(0x6F, 0x13, 0x11);
	// Same priority as both.
	runCase(0x30, 0x30, 0x30);

	return 0;
}
