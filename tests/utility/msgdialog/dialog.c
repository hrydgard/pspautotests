#define sceUtilityMsgDialogShutdownStart sceUtilityMsgDialogShutdownStart_WRONG

#include <common.h>
#include <pspgu.h>
#include <pspdisplay.h>
#include <psputility.h>
#include <psputility_msgdialog.h>

#undef sceUtilityMsgDialogShutdownStart

int sceUtilityMsgDialogShutdownStart();

unsigned int __attribute__((aligned(16))) list[262144];

void init() {
	sceGuInit();
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888, 0, 512);
	sceGuDispBuffer(480, 272, 0, 512);
	sceGuScissor(0, 0, 480, 272);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFinish();
	sceGuSync(0, 0);
 
	sceDisplaySetMode(0, 480, 272);
	sceDisplayWaitVblankStart();
	sceGuDisplay(1);
}

int getStatus(pspUtilityMsgDialogParams *dialog) {
	static int lastStatus = -1;
	static int c = 0;

	static int lastResult = -0x1337;
	static int lastButton = -0x1337;
	static int lastError = -0x1337;

	int status = sceUtilityMsgDialogGetStatus();
	++c;
	if (status != lastStatus) {
		checkpoint("GetStatus after %d: %08x", c, status);
		lastStatus = status;
		c = 0;
	}

	if (lastResult != dialog->unknown) {
		checkpoint("Result -> %08x", dialog->unknown);
		lastResult = dialog->unknown;
	}

	if (lastButton != dialog->buttonPressed) {
		checkpoint("Button -> %08x", dialog->buttonPressed);
		lastButton = dialog->buttonPressed;
	}

	if (lastError != dialog->errorValue) {
		checkpoint("Error -> %08x", dialog->errorValue);
		lastError = dialog->errorValue;
	}

	return status;
}

int main(int argc, char *argv[]) {
	pspUtilityMsgDialogParams dialog;
	memset(&dialog, 0, sizeof(dialog));
	dialog.unknown = 0x1337;
	dialog.errorValue = 0x1337;
	dialog.buttonPressed = (pspUtilityMsgDialogPressed)0x1337;

	init();

	dialog.base.size = sizeof(dialog);
	dialog.base.language = 1;
	dialog.base.buttonSwap = 1;
	dialog.base.soundThread = 0x23;
	dialog.base.graphicsThread = 0x24;
	dialog.base.accessThread = 0x25;
	dialog.base.fontThread = 0x26;
	dialog.mode = PSP_UTILITY_MSGDIALOG_MODE_TEXT;
	strcpy(dialog.message, "11111111111111111111111111111111111112111111111111111111111");

	checkpoint("InitStart: %08x", sceUtilityMsgDialogInitStart(&dialog));
	getStatus(&dialog);

	static int j = 0;
	while (getStatus(&dialog) < 3 && j++ < 100000) {
		sceUtilityMsgDialogUpdate(1);
		sceDisplayWaitVblankStart();
		if (j == 500) {
			checkpoint("Abort: %08x", sceUtilityMsgDialogAbort());
		}
	}

	checkpoint("ShutdownStart: %08x", sceUtilityMsgDialogShutdownStart());
	while (getStatus(&dialog) == 3 && j++ < 100000) {
		continue;
	}
	sceKernelDelayThread(500000);
	getStatus(&dialog);
	getStatus(&dialog);

	return 0;
}