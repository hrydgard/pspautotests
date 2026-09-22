#include "shared.h"
#include <string.h>

// Which dialog type a PSP considers current, as seen through each type's GetStatus, and what the
// other types' calls return while one is running, finished or shutting down.

static void msgCallsOutsideDialog(const char *label) {
	printf("%s:\n", label);
	printf("  msg Update: %s\n", statusName(sceUtilityMsgDialogUpdate(1)));
	printf("  msg ShutdownStart: %s\n", statusName(sceUtilityMsgDialogShutdownStart()));
	printf("  msg Abort: %s\n", statusName(sceUtilityMsgDialogAbort()));
}

static void tryMsgInitStart(const char *label) {
	MsgDialogParams msg;
	initMsgParams(&msg, 0x24, 0x25);
	int result = sceUtilityMsgDialogInitStart((pspUtilityMsgDialogParams *)&msg);
	printf("%s: msg InitStart: %s\n", label, statusName(result));
	if (result >= 0) {
		printf("  (started, finishing it: %s)\n", finishMsg() >= 0 ? "ok" : "failed");
	}
}

static void trySavedataInitStart(const char *label) {
	SavedataParams save;
	initSavedataParams(&save);
	int result = sceUtilitySavedataInitStart((SceUtilitySavedataParam *)&save);
	printf("%s: savedata InitStart: %s\n", label, statusName(result));
	if (result >= 0) {
		printf("  (started, finishing it: %s)\n", finishSavedata() >= 0 ? "ok" : "failed");
	}
}

int main(int argc, char *argv[]) {
	initDisplay();

	printAllStatus("At boot");
	msgCallsOutsideDialog("At boot");

	// A GameSharing InitStart that fails on its size.
	pspUtilityGameSharingParams share;
	memset(&share, 0, sizeof(share));
	share.base.size = 4;
	printf("GameSharing InitStart, bad size: %s\n", statusName(sceUtilityGameSharingInitStart(&share)));
	printAllStatus("After failed GameSharing InitStart");

	// A msg InitStart that fails on its size.
	MsgDialogParams msg;
	initMsgParams(&msg, 0x24, 0x25);
	msg.base.size = 4;
	printf("msg InitStart, bad size: %s\n", statusName(sceUtilityMsgDialogInitStart((pspUtilityMsgDialogParams *)&msg)));
	printAllStatus("After failed msg InitStart");

	// Savedata, and everything else while it's in each state.
	SavedataParams save;
	initSavedataParams(&save);
	printf("savedata InitStart: %s\n", statusName(sceUtilitySavedataInitStart((SceUtilitySavedataParam *)&save)));
	printAllStatus("Savedata just started");
	msgCallsOutsideDialog("Savedata just started");
	tryMsgInitStart("Savedata just started");
	share.base.size = 4;
	printf("Savedata just started: GameSharing InitStart, bad size: %s\n", statusName(sceUtilityGameSharingInitStart(&share)));

	int frame = 0;
	while (sceUtilitySavedataGetStatus() == 1 && frame++ < 600) {
		sceDisplayWaitVblankStart();
	}
	printAllStatus("Savedata running");
	msgCallsOutsideDialog("Savedata running");
	tryMsgInitStart("Savedata running");

	while (sceUtilitySavedataGetStatus() == 2 && frame++ < 600) {
		sceUtilitySavedataUpdate(1);
		sceDisplayWaitVblankStart();
	}
	printAllStatus("Savedata finished");
	msgCallsOutsideDialog("Savedata finished");
	tryMsgInitStart("Savedata finished");
	trySavedataInitStart("Savedata finished");

	printf("savedata ShutdownStart: %s\n", statusName(sceUtilitySavedataShutdownStart()));
	printAllStatus("Savedata shutting down");
	tryMsgInitStart("Savedata shutting down");
	printf("savedata ShutdownStart again: %s\n", statusName(sceUtilitySavedataShutdownStart()));

	while (sceUtilitySavedataGetStatus() != 0 && frame++ < 600) {
		sceDisplayWaitVblankStart();
	}
	printAllStatus("Savedata done");
	msgCallsOutsideDialog("Savedata done");
	printf("savedata ShutdownStart after done: %s\n", statusName(sceUtilitySavedataShutdownStart()));
	printf("savedata Update after done: %s\n", statusName(sceUtilitySavedataUpdate(1)));
	printAllStatus("After those");

	// Now a msg dialog, so the last type started changes.
	initMsgParams(&msg, 0x24, 0x25);
	printf("msg InitStart: %s\n", statusName(sceUtilityMsgDialogInitStart((pspUtilityMsgDialogParams *)&msg)));
	printAllStatus("Msg just started");
	printf("msg finished: %s\n", finishMsg() >= 0 ? "ok" : "failed");
	printAllStatus("Msg done");

	// And a failed GameSharing start right after, to see whether a failure changes the type.
	share.base.size = 4;
	printf("GameSharing InitStart, bad size: %s\n", statusName(sceUtilityGameSharingInitStart(&share)));
	printAllStatus("After failed GameSharing InitStart");
	printf("GameSharing Update: %s\n", statusName(sceUtilityGameSharingUpdate(1)));
	printf("GameSharing ShutdownStart: %s\n", statusName(sceUtilityGameSharingShutdownStart()));

	// Update during INIT, and ShutdownStart before the dialog has finished.
	initMsgParams(&msg, 0x24, 0x25);
	printf("msg InitStart: %s\n", statusName(sceUtilityMsgDialogInitStart((pspUtilityMsgDialogParams *)&msg)));
	printf("msg Update during INIT: %s\n", statusName(sceUtilityMsgDialogUpdate(1)));
	printf("msg ShutdownStart during INIT: %s\n", statusName(sceUtilityMsgDialogShutdownStart()));
	frame = 0;
	while (sceUtilityMsgDialogGetStatus() == 1 && frame++ < 600) {
		sceDisplayWaitVblankStart();
	}
	printf("msg status: %s\n", statusName(sceUtilityMsgDialogGetStatus()));
	int result = sceUtilityMsgDialogShutdownStart();
	printf("msg ShutdownStart while RUNNING: %s\n", statusName(result));
	printf("msg status: %s\n", statusName(sceUtilityMsgDialogGetStatus()));
	if (result >= 0) {
		while (sceUtilityMsgDialogGetStatus() != 0 && frame++ < 600) {
			sceDisplayWaitVblankStart();
		}
		printf("msg status: %s\n", statusName(sceUtilityMsgDialogGetStatus()));
	} else {
		printf("msg finished: %s\n", finishMsg() >= 0 ? "ok" : "failed");
	}
	printf("msg Update after done: %s\n", statusName(sceUtilityMsgDialogUpdate(1)));
	printf("msg Abort after done: %s\n", statusName(sceUtilityMsgDialogAbort()));

	return 0;
}
