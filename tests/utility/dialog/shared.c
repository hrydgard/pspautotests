#include "shared.h"
#include <string.h>

static unsigned int __attribute__((aligned(16))) list[262144];
static char sizeInfoBuf[256];

void initDisplay() {
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

void initMsgParams(MsgDialogParams *p, int graphicsThread, int accessThread) {
	memset(p, 0, sizeof(*p));
	p->base.size = MSGDIALOG_SIZE_V3;
	p->base.language = 1;
	p->base.buttonSwap = 1;
	p->base.soundThread = 0x23;
	p->base.graphicsThread = graphicsThread;
	p->base.accessThread = accessThread;
	p->base.fontThread = 0x26;
	p->mode = PSP_UTILITY_MSGDIALOG_MODE_TEXT;
	strcpy(p->message, "Test");
}

void initSavedataParams(SavedataParams *p) {
	memset(p, 0, sizeof(*p));
	memset(sizeInfoBuf, 0, sizeof(sizeInfoBuf));
	p->base.size = sizeof(*p);
	p->base.language = 1;
	p->base.buttonSwap = 1;
	p->base.graphicsThread = 0x21;
	p->base.accessThread = 0x22;
	p->base.fontThread = 0x23;
	p->base.soundThread = 0x20;
	p->mode = 22;
	strcpy(p->gameName, "TEST99902");
	strcpy(p->saveName, "ABC");
	strcpy(p->fileName, "DATA.BIN");
	p->sizeInfo = sizeInfoBuf;
}

const char *statusName(int status) {
	switch (status) {
	case 0: return "NONE";
	case 1: return "INIT";
	case 2: return "RUNNING";
	case 3: return "FINISHED";
	case 4: return "SHUTDOWN";
	case (int)0x80110001: return "INVALID_STATUS";
	case (int)0x80110004: return "INVALID_PARAM_SIZE";
	case (int)0x80110005: return "WRONG_TYPE";
	default: {
		static char buf[16];
		sprintf(buf, "%08x", status);
		return buf;
	}
	}
}

void printAllStatus(const char *label) {
	printf("%s:\n", label);
	printf("  msg=%s", statusName(sceUtilityMsgDialogGetStatus()));
	printf(" save=%s", statusName(sceUtilitySavedataGetStatus()));
	printf(" osk=%s", statusName(sceUtilityOskGetStatus()));
	printf(" net=%s", statusName(sceUtilityNetconfGetStatus()));
	printf(" share=%s\n", statusName(sceUtilityGameSharingGetStatus()));
	printf("  html=%s", statusName(sceUtilityHtmlViewerGetStatus()));
	printf(" shot=%s", statusName(sceUtilityScreenshotGetStatus()));
	printf(" install=%s", statusName(sceUtilityGamedataInstallGetStatus()));
	printf(" np=%s\n", statusName(sceUtilityNpSigninGetStatus()));
}

static int waitForNone(int (*getStatus)(), int frame) {
	while (getStatus() != 0 && frame < 600) {
		sceDisplayWaitVblankStart();
		frame++;
	}
	return getStatus() == 0 ? frame : -1;
}

int finishMsg() {
	int frame = 0;
	int aborted = 0;
	while (frame < 600) {
		int status = sceUtilityMsgDialogGetStatus();
		if (status == 3 || status < 0) {
			break;
		}
		if (status == 2 && !aborted) {
			sceUtilityMsgDialogAbort();
			aborted = 1;
		}
		if (status == 2) {
			sceUtilityMsgDialogUpdate(1);
		}
		sceDisplayWaitVblankStart();
		frame++;
	}
	if (sceUtilityMsgDialogGetStatus() != 3) {
		return -1;
	}
	sceUtilityMsgDialogShutdownStart();
	return waitForNone(&sceUtilityMsgDialogGetStatus, frame);
}

int finishSavedata() {
	int frame = 0;
	while (frame < 600) {
		int status = sceUtilitySavedataGetStatus();
		if (status == 3 || status < 0) {
			break;
		}
		if (status == 2) {
			sceUtilitySavedataUpdate(1);
		}
		sceDisplayWaitVblankStart();
		frame++;
	}
	if (sceUtilitySavedataGetStatus() != 3) {
		return -1;
	}
	sceUtilitySavedataShutdownStart();
	return waitForNone(&sceUtilitySavedataGetStatus, frame);
}
