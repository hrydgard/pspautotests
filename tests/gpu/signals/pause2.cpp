#include <common.h>

#include <stdlib.h>
#include <stdio.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspthreadman.h>

extern "C" {
#include "sysmem-imports.h"
#include "../commands/commands.h"
}

static uint32_t __attribute__((aligned(16))) dlist1[] = {
	(GE_CMD_NOP << 24) | 0x000000,
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000001,
	(GE_CMD_SIGNAL << 24) | (PSP_GE_SIGNAL_HANDLER_PAUSE << 16) | 0x0064,
	(GE_CMD_END << 24) | 0x000000,
	(GE_CMD_FINISH << 24) | 0x000000,
	(GE_CMD_END << 24) | 0x000000,
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000002,
	(GE_CMD_FINISH << 24) | 0x000000,
	(GE_CMD_END << 24) | 0x000000,
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000003,
	(GE_CMD_FINISH << 24) | 0x000000,
	(GE_CMD_END << 24) | 0x000000,
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000004,
	(GE_CMD_FINISH << 24) | 0x000000,
	(GE_CMD_END << 24) | 0x000000,
};

static uint32_t __attribute__((aligned(16))) dlist2[] = {
	(GE_CMD_NOP << 24) | 0x000000,
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000001,
	(GE_CMD_AMBIENTALPHA << 24) | 0x000001,
	(GE_CMD_SIGNAL << 24) | (PSP_GE_SIGNAL_HANDLER_PAUSE << 16) | 0x0064,
	(GE_CMD_END << 24) | 0x000000,
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000002,
	(GE_CMD_FINISH << 24) | 0x000000,
	(GE_CMD_END << 24) | 0x000000,
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000003,
	(GE_CMD_FINISH << 24) | 0x000000,
	(GE_CMD_END << 24) | 0x000000,
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000004,
	(GE_CMD_FINISH << 24) | 0x000000,
	(GE_CMD_END << 24) | 0x000000,
};

static uint32_t __attribute__((aligned(16))) dlist3[] = {
	(GE_CMD_NOP << 24) | 0x000000,
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000010,
	(GE_CMD_AMBIENTALPHA << 24) | 0x000010,
	(GE_CMD_FINISH << 24) | 0x000000,
	(GE_CMD_END << 24) | 0x000000,
};

static int currentListID = 0;
static uint32_t *currentListStart = NULL;

static int ge_signal(int value, void *arg, u32 *listpc) {
	if (listpc != NULL) {
		checkpoint("  * ge_signal: %08x +%08x", arg, listpc - currentListStart);
	} else {
		checkpoint("  * ge_signal: %08x %08x", arg, listpc);
	}
	checkpoint("  * ge_signal: %d at %x, listsync: %08x, drawsync: %08x", value, sceGeGetCmd(GE_CMD_AMBIENTCOLOR), sceGeListSync(currentListID, 1), sceGeDrawSync(1));
	return 0;
}

static int ge_signal2(int value, void *arg, u32 *listpc) {
	if (listpc != NULL) {
		checkpoint("  * ge_signal: %08x +%08x", arg, listpc - currentListStart);
	} else {
		checkpoint("  * ge_signal: %08x %08x", arg, listpc);
	}
	checkpoint("  * ge_signal: %d at %x, listsync: %08x, drawsync: %08x", value, sceGeGetCmd(GE_CMD_AMBIENTCOLOR), sceGeListSync(currentListID, 1), sceGeDrawSync(1));
	checkpoint("  * Continue: %08x", sceGeContinue());
	return 0;
}

static int ge_finish(int value, void *arg, u32 *listpc) {
	if (listpc != NULL) {
		checkpoint("  * ge_signal: %08x +%08x", arg, listpc - currentListStart);
	} else {
		checkpoint("  * ge_signal: %08x %08x", arg, listpc);
	}
	checkpoint("  * ge_finish: %d at %x, listsync: %08x, drawsync: %08x", value, sceGeGetCmd(GE_CMD_AMBIENTCOLOR), sceGeListSync(currentListID, 1), sceGeDrawSync(1));
	return 0;
}

static void testPauseSdkVersion(u32 ver) {
	char temp[256];
	snprintf(temp, sizeof(temp), "SDK version %08x", ver);
	checkpointNext(temp);
	if (ver != 0) {
		sceKernelSetCompiledSdkVersion(ver);
	}

	PspGeCallbackData cbdata;

	cbdata.signal_func = (PspGeCallback) ge_signal;
	cbdata.signal_arg  = NULL;
	cbdata.finish_func = (PspGeCallback) ge_finish;
	cbdata.finish_arg  = NULL;
	int cbid1 = sceGeSetCallback(&cbdata);

	currentListStart = dlist1;
	currentListID = sceGeListEnQueue(dlist1, dlist1, cbid1, NULL);
	sceGeListUpdateStallAddr(currentListID, dlist1 + 100);
	sceKernelDelayThread(10000);
	checkpoint("  UpdateStallAddr: %08x", sceGeListUpdateStallAddr(currentListID, dlist1 + 100));
	checkpoint("  Continue: %08x", sceGeContinue());
	sceKernelDelayThread(10000);
	sceGeBreak(1, NULL);
	checkpoint("  Callback 1 done: %x", sceGeGetCmd(GE_CMD_AMBIENTCOLOR));

	sceGeUnsetCallback(cbid1);

	cbdata.signal_func = (PspGeCallback) ge_signal2;
	cbdata.signal_arg  = NULL;
	cbdata.finish_func = (PspGeCallback) ge_finish;
	cbdata.finish_arg  = NULL;
	int cbid2 = sceGeSetCallback(&cbdata);

	currentListStart = dlist1;
	currentListID = sceGeListEnQueue(dlist1, dlist1, cbid2, NULL);
	sceGeListUpdateStallAddr(currentListID, dlist1 + 100);
	sceKernelDelayThread(10000);
	checkpoint("  UpdateStallAddr: %08x", sceGeListUpdateStallAddr(currentListID, dlist1 + 100));
	checkpoint("  Continue: %08x", sceGeContinue());
	sceKernelDelayThread(10000);
	sceGeBreak(1, NULL);
	checkpoint("  Callback 2 done: %x", sceGeGetCmd(GE_CMD_AMBIENTCOLOR));
	
	sceGeUnsetCallback(cbid2);
}

static void testPauseHead() {
	checkpointNext("Pause with queue head");

	PspGeCallbackData cbdata;

	cbdata.signal_func = (PspGeCallback)ge_signal;
	cbdata.signal_arg = NULL;
	cbdata.finish_func = (PspGeCallback)ge_finish;
	cbdata.finish_arg = NULL;
	int cbid1 = sceGeSetCallback(&cbdata);

	currentListStart = dlist2;
	currentListID = sceGeListEnQueue(dlist2, (uint8_t *)dlist2 + sizeof(dlist2), cbid1, NULL);
	sceKernelDelayThread(10000);
	checkpoint("  After pause: %x / %x", sceGeGetCmd(GE_CMD_AMBIENTCOLOR), sceGeGetCmd(GE_CMD_AMBIENTALPHA));

	int listid2 = sceGeListEnQueueHead(dlist3, (uint8_t *)dlist3 + sizeof(dlist3), -1, NULL);
	sceKernelDelayThread(10000);
	checkpoint("  After enqueue head: %x / %x", sceGeGetCmd(GE_CMD_AMBIENTCOLOR), sceGeGetCmd(GE_CMD_AMBIENTALPHA));

	checkpoint("  Continue: %08x", sceGeContinue());
	sceKernelDelayThread(10000);
	sceGeBreak(1, NULL);
	checkpoint("  Final list: %x / %x", sceGeGetCmd(GE_CMD_AMBIENTCOLOR), sceGeGetCmd(GE_CMD_AMBIENTALPHA));

	sceGeUnsetCallback(cbid1);
}

extern "C" int main(int argc, char *argv[]) {
	testPauseSdkVersion(0);
	testPauseSdkVersion(0x02080000);
	testPauseSdkVersion(0x03080000);
	testPauseSdkVersion(0x06060010);

	sceKernelSetCompiledSdkVersion(0x06060010);
	testPauseHead();

	return 0;
}