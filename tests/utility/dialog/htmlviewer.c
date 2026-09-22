#include "shared.h"
#include <string.h>

// Which sizes HtmlViewer InitStart accepts, and that it isn't blocked by other dialogs. A valid
// start allocates 3.5MB of user memory; the test leaves less than that free, so it fails with
// 800200d9 and the browser never starts.

int sceUtilityHtmlViewerUpdate(int animSpeed);
int sceUtilityHtmlViewerShutdownStart();

typedef struct {
	pspUtilityDialogCommon base;
	void *memaddr;
	unsigned int memsize;
	int unknown1;
	int unknown2;
	char *initialurl;
	unsigned int numtabs;
	unsigned int interfacemode;
	unsigned int options;
	char *dldirname;
	char *dlfilename;
	char *uldirname;
	char *ulfilename;
	unsigned int cookiemode;
	unsigned int unknown3;
	char *homeurl;
	unsigned int textsize;
	unsigned int displaymode;
	unsigned int connectmode;
	unsigned int disconnectmode;
	unsigned int memused;
	int unknown4[10];
	char pad[256];
} HtmlViewerParams;

static HtmlViewerParams params;

static void initParams(unsigned int size) {
	memset(&params, 0, sizeof(params));
	params.base.size = size;
	params.base.language = 1;
	params.base.buttonSwap = 1;
	params.base.graphicsThread = 0x21;
	params.base.accessThread = 0x22;
	params.base.fontThread = 0x23;
	params.base.soundThread = 0x20;
	params.initialurl = "http://www.example.com/";
	params.numtabs = 1;
}

// If one ever starts, give the player 10 seconds to back out, then shut it down.
static void finishIfStarted(int result) {
	if (result < 0) {
		return;
	}
	printf("  STARTED\n");
	int frame = 0;
	while (frame++ < 600) {
		int status = sceUtilityHtmlViewerGetStatus();
		if (status == 3 || status < 0) {
			break;
		}
		if (status == 2) {
			sceUtilityHtmlViewerUpdate(1);
		}
		sceDisplayWaitVblankStart();
	}
	printf("  status: %s, result %08x\n", statusName(sceUtilityHtmlViewerGetStatus()), params.base.result);
	if (sceUtilityHtmlViewerGetStatus() == 3) {
		sceUtilityHtmlViewerShutdownStart();
		while (sceUtilityHtmlViewerGetStatus() != 0 && frame++ < 900) {
			sceDisplayWaitVblankStart();
		}
	}
}

int main(int argc, char *argv[]) {
	initDisplay();

	// Whatever the environment leaves free, make it less than the 0x380000 the browser needs.
	SceUID pad = -1;
	SceSize freeSize = sceKernelMaxFreeMemSize();
	if (freeSize > 0x300000) {
		pad = sceKernelAllocPartitionMemory(2, "pad", PSP_SMEM_Low, freeSize - 0x300000, NULL);
	}

	unsigned int size;
	int last = 0x12345678;
	for (size = 0; size <= 0x200; size += 4) {
		initParams(size);
		int result = sceUtilityHtmlViewerInitStart((pspUtilityHtmlViewerParam *)&params);
		if (result != last) {
			printf("size %03x: %s\n", size, statusName(result));
			last = result;
		}
		finishIfStarted(result);
		if (result >= 0) {
			break;
		}
	}

	printAllStatus("After");

	// HtmlViewer keeps its own state: a msg dialog doesn't make it busy.
	MsgDialogParams msg;
	initMsgParams(&msg, 0x24, 0x25);
	printf("msg InitStart: %s\n", statusName(sceUtilityMsgDialogInitStart((pspUtilityMsgDialogParams *)&msg)));
	initParams(0xA8);
	int result = sceUtilityHtmlViewerInitStart((pspUtilityHtmlViewerParam *)&params);
	printf("HtmlViewer InitStart during msg: %s\n", statusName(result));
	finishIfStarted(result);
	printf("HtmlViewer Update: %s\n", statusName(sceUtilityHtmlViewerUpdate(1)));
	printf("HtmlViewer ShutdownStart: %s\n", statusName(sceUtilityHtmlViewerShutdownStart()));
	printAllStatus("During msg");
	printf("msg finished: %s\n", finishMsg() >= 0 ? "ok" : "failed");

	if (pad >= 0) {
		sceKernelFreePartitionMemory(pad);
	}
	return 0;
}
