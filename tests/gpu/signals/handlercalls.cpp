#include <common.h>
#include <pspge.h>
#include <pspthreadman.h>
#include <psputils.h>

extern "C" {
#include "sysmem-imports.h"
#include "../commands/commands.h"
}

// A SUSPEND signal callback runs with the GE stopped right after the SIGNAL/END.  What do
// sceGeBreak and sceGeListUpdateStallAddr do when they're called from in there?
//
// Not here, on purpose: sceGeBreak(0) from a SUSPEND handler with an SDK version above 0x02000010.
// There the list isn't PAUSED during the handler, so the break goes through, and the GE is
// restarted regardless once the handler returns - on nothing in particular.  That hung a PSP
// hard enough that PSPLink's reset couldn't bring it back.

static u32 __attribute__((aligned(16))) list[32];

static int listID;
static volatile int signalCount;
static volatile int finishCount;

enum HandlerAction {
	ACTION_BREAK,
	ACTION_UPDATE_STALL,
};
static HandlerAction action;

static int signalCallback(int value, void *arg, u32 *listpc) {
	signalCount++;
	if (signalCount > 1) {
		checkpoint("  * signal %d again, listsync: %08x", value, sceGeListSync(listID, 1));
		return 0;
	}

	checkpoint("  * signal %d, listsync: %08x, at %x", value, sceGeListSync(listID, 1), sceGeGetCmd(GE_CMD_AMBIENTCOLOR) & 0xFF);
	if (action == ACTION_BREAK) {
		int result = sceGeBreak(0, NULL);
		checkpoint("  * sceGeBreak(0): %s, listsync: %08x", result == listID ? "list id" : "other", sceGeListSync(listID, 1));
		if (result != listID) {
			checkpoint("  * sceGeBreak(0) result: %08x", result);
		}
	} else {
		checkpoint("  * update stall: %08x, listsync: %08x", sceGeListUpdateStallAddr(listID, 0), sceGeListSync(listID, 1));
	}
	return 0;
}

static int finishCallback(int value, void *arg, u32 *listpc) {
	finishCount++;
	checkpoint("  * finish %d, at %x", value, sceGeGetCmd(GE_CMD_AMBIENTCOLOR) & 0xFF);
	return 0;
}

static void runTest(const char *title, u32 ver, HandlerAction act) {
	checkpointNext(title);
	// DO NOT remove this to "complete the matrix", it hangs real hardware.  See the top of the file:
	// above this SDK version the list isn't PAUSED during the handler, so sceGeBreak(0) goes through
	// instead of failing with 0x80000021, and the PSP needs a power cycle afterwards.
	if (act == ACTION_BREAK && ver > 0x02000010) {
		checkpoint("  Skipped, this would hang a PSP");
		return;
	}
	sceKernelSetCompiledSdkVersion(ver);
	action = act;
	signalCount = 0;
	finishCount = 0;

	for (int i = 0; i < 32; ++i) {
		list[i] = GE_CMD_NOP << 24;
	}
	list[0] = (GE_CMD_AMBIENTCOLOR << 24) | 1;
	list[1] = (GE_CMD_SIGNAL << 24) | (PSP_GE_SIGNAL_HANDLER_SUSPEND << 16) | 0x0042;
	list[2] = GE_CMD_END << 24;
	list[3] = (GE_CMD_AMBIENTCOLOR << 24) | 2;
	// When updating the stall address, this is where it's stalled before.
	list[6] = (GE_CMD_AMBIENTCOLOR << 24) | 3;
	list[7] = (GE_CMD_FINISH << 24) | 0x0011;
	list[8] = GE_CMD_END << 24;
	sceKernelDcacheWritebackRange(list, sizeof(list));

	PspGeCallbackData cbdata;
	cbdata.signal_func = (PspGeCallback)signalCallback;
	cbdata.signal_arg = NULL;
	cbdata.finish_func = (PspGeCallback)finishCallback;
	cbdata.finish_arg = NULL;
	int cbid = sceGeSetCallback(&cbdata);

	// Stalled at the start, or the signal would arrive before we even know the list's id.
	listID = sceGeListEnQueue(list, list, cbid, NULL);
	sceGeListUpdateStallAddr(listID, act == ACTION_UPDATE_STALL ? list + 5 : NULL);
	sceKernelDelayThread(10000);
	checkpoint("  After signal: listsync: %08x, drawsync: %08x, at %x, finished: %d", sceGeListSync(listID, 1), sceGeDrawSync(1), sceGeGetCmd(GE_CMD_AMBIENTCOLOR) & 0xFF, finishCount);

	if (act == ACTION_BREAK) {
		int result = sceGeContinue();
		sceKernelDelayThread(10000);
		checkpoint("  sceGeContinue: %08x", result);
	} else {
		int result = sceGeListUpdateStallAddr(listID, 0);
		sceKernelDelayThread(10000);
		checkpoint("  Update stall again: %08x", result);
	}
	checkpoint("  After: listsync: %08x, at %x, signals: %d, finished: %d", sceGeListSync(listID, 1), sceGeGetCmd(GE_CMD_AMBIENTCOLOR) & 0xFF, signalCount, finishCount);

	sceGeBreak(1, NULL);
	sceGeUnsetCallback(cbid);
}

extern "C" int main(int argc, char *argv[]) {
	runTest("Break in signal handler, old SDK:", 0x01000010, ACTION_BREAK);
	runTest("Update stall in signal handler, old SDK:", 0x01000010, ACTION_UPDATE_STALL);
	runTest("Update stall in signal handler, new SDK:", 0x06060010, ACTION_UPDATE_STALL);
	return 0;
}
