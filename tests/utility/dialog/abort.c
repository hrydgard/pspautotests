#include "shared.h"
#include <string.h>

// How many Updates a msg dialog takes to reach FINISHED after Abort, depending on how far into
// the fade-in it was aborted, and what it reports.

static void waitVblanks(int n) {
	int i;
	for (i = 0; i < n; ++i) {
		sceDisplayWaitVblankStart();
	}
}

static void runCase(int updatesBefore, int animSpeed, int vblanksPerUpdate) {
	MsgDialogParams msg;
	initMsgParams(&msg, 0x24, 0x25);
	msg.result = 0x1337;
	msg.buttonPressed = 0x1337;
	sceUtilityMsgDialogInitStart((pspUtilityMsgDialogParams *)&msg);

	int frame = 0;
	while (sceUtilityMsgDialogGetStatus() == 1 && frame++ < 600) {
		sceDisplayWaitVblankStart();
	}
	int i;
	for (i = 0; i < updatesBefore; ++i) {
		sceUtilityMsgDialogUpdate(animSpeed);
		waitVblanks(vblanksPerUpdate);
	}

	int abortResult = sceUtilityMsgDialogAbort();
	int statusAfterAbort = sceUtilityMsgDialogGetStatus();
	int updates = 0;
	while (sceUtilityMsgDialogGetStatus() == 2 && updates < 300) {
		sceUtilityMsgDialogUpdate(animSpeed);
		updates++;
		waitVblanks(vblanksPerUpdate);
	}
	int status = sceUtilityMsgDialogGetStatus();
	printf("animSpeed %d, %d vblanks per update, abort after %2d updates: %s, then %s; %s after %d updates, result=%08x button=%08x\n", animSpeed, vblanksPerUpdate, updatesBefore,
		statusName(abortResult), statusName(statusAfterAbort), statusName(status), updates, msg.result, msg.buttonPressed);

	if (status == 3) {
		sceUtilityMsgDialogShutdownStart();
	}
	while (sceUtilityMsgDialogGetStatus() != 0 && frame++ < 900) {
		sceDisplayWaitVblankStart();
	}
}

int main(int argc, char *argv[]) {
	initDisplay();

	static const int cases[] = { 0, 2, 5, 10 };
	int i;
	for (i = 0; i < (int)ARRAY_SIZE(cases); ++i) {
		runCase(cases[i], 1, 1);
	}
	// animSpeed doesn't change it. (An Update every other vblank took 6 Updates rather than 8, so
	// it isn't simply counting Updates either - not covered here.)
	runCase(0, 2, 1);
	return 0;
}
