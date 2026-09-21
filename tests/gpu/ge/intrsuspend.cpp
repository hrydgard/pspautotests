#include <common.h>
#include <pspge.h>
#include <pspintrman.h>
#include <pspthreadman.h>
#include <psputils.h>

#include "../commands/commands.h"

extern "C" {
#include "sysmem-imports.h"
}

// The GE stops at a FINISH, and it's the interrupt that moves the queue along.  So what does the
// queue look like while that interrupt can't be taken?  And which contexts can wait for the GE?

static u32 __attribute__((aligned(16))) lists[2][32];

static volatile int finishCount;
static volatile int finishOrder[4];
static int listIDs[2];
static int cbListSync, cbDrawSync;

static int finishCallback(int value, void *arg, u32 *listpc) {
	if (finishCount < 4) {
		finishOrder[finishCount] = value;
	}
	if (finishCount == 0) {
		// Nothing can wait in an interrupt, even for something that's already done.
		cbListSync = sceGeListSync(listIDs[0], 0);
		cbDrawSync = sceGeDrawSync(0);
	}
	finishCount++;
	return 0;
}

static void resetLists() {
	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < 32; ++j) {
			lists[i][j] = GE_CMD_NOP << 24;
		}
		lists[i][8] = (GE_CMD_FINISH << 24) | (i + 1);
		lists[i][9] = GE_CMD_END << 24;
	}
	sceKernelDcacheWritebackRange(lists, sizeof(lists));
	finishCount = 0;
	for (int i = 0; i < 4; ++i) {
		finishOrder[i] = 0;
	}
}

// Can't sleep with interrupts off, and the GE needs a moment on hardware.
static void spin() {
	for (volatile int i = 0; i < 2000000; ++i) {
		continue;
	}
}

static void testQueueWhileSuspended(int cbid) {
	resetLists();

	// No output until interrupts are back on, that needs them.
	unsigned int flags = sceKernelCpuSuspendIntr();
	listIDs[0] = sceGeListEnQueue(lists[0], 0, cbid, NULL);
	listIDs[1] = sceGeListEnQueue(lists[1], 0, cbid, NULL);
	spin();
	int sync0 = sceGeListSync(listIDs[0], 1);
	int sync1 = sceGeListSync(listIDs[1], 1);
	int draw = sceGeDrawSync(1);
	int count = finishCount;
	int waitList = sceGeListSync(listIDs[0], 0);
	int waitDraw = sceGeDrawSync(0);
	int countAfterWait = finishCount;
	sceKernelCpuResumeIntr(flags);

	sceKernelDelayThread(10000);
	checkpoint("  Suspended, first list: %08x, second: %08x, drawsync: %08x", sync0, sync1, draw);
	checkpoint("  Suspended, finish callbacks: %d", count);
	checkpoint("  Suspended, sceGeListSync(0): %08x, sceGeDrawSync(0): %08x, callbacks after: %d", waitList, waitDraw, countAfterWait);
	checkpoint("  Resumed, first list: %08x, second: %08x, drawsync: %08x", sceGeListSync(listIDs[0], 1), sceGeListSync(listIDs[1], 1), sceGeDrawSync(1));
	checkpoint("  Resumed, finish callbacks: %d, order: %d %d", finishCount, finishOrder[0], finishOrder[1]);
	checkpoint("  In first callback, sceGeListSync(0): %08x, sceGeDrawSync(0): %08x", cbListSync, cbDrawSync);
	sceGeBreak(1, NULL);
}

static void testBreakWhileSuspended(int cbid) {
	resetLists();

	unsigned int flags = sceKernelCpuSuspendIntr();
	listIDs[0] = sceGeListEnQueue(lists[0], 0, cbid, NULL);
	spin();
	// The list has hit its FINISH by now, and the interrupt for that is waiting.
	int result = sceGeBreak(1, NULL);
	int sync0 = sceGeListSync(listIDs[0], 1);
	sceKernelCpuResumeIntr(flags);

	sceKernelDelayThread(10000);
	checkpoint("  sceGeBreak(1) with a finish pending: %08x, list after: %08x", result, sync0);
	checkpoint("  Finish callbacks once resumed: %d", finishCount);

	// And is the queue in working order afterwards?
	listIDs[0] = sceGeListEnQueue(lists[1], 0, cbid, NULL);
	sceKernelDelayThread(10000);
	checkpoint("  Next list: %08x, finish callbacks: %d, last value: %d", sceGeListSync(listIDs[0], 1), finishCount, finishCount > 0 ? finishOrder[finishCount - 1] : 0);
	sceGeBreak(1, NULL);
}

static void testDispatchSuspended(int cbid) {
	resetLists();

	int state = sceKernelSuspendDispatchThread();
	listIDs[0] = sceGeListEnQueue(lists[0], 0, cbid, NULL);
	spin();
	// Interrupts are still on, so this one does complete, callback and all.
	int count = finishCount;
	int sync0 = sceGeListSync(listIDs[0], 1);
	int waitList = sceGeListSync(listIDs[0], 0);
	int waitDraw = sceGeDrawSync(0);
	sceKernelResumeDispatchThread(state);

	checkpoint("  Dispatch suspended, finish callbacks: %d, list: %08x", count, sync0);
	checkpoint("  Dispatch suspended, sceGeListSync(0): %08x, sceGeDrawSync(0): %08x", waitList, waitDraw);
	sceGeBreak(1, NULL);
}

extern "C" int main(int argc, char *argv[]) {
	sceKernelSetCompiledSdkVersion(0x06060010);

	PspGeCallbackData cbdata;
	cbdata.signal_func = NULL;
	cbdata.signal_arg = NULL;
	cbdata.finish_func = (PspGeCallback)finishCallback;
	cbdata.finish_arg = NULL;
	int cbid = sceGeSetCallback(&cbdata);

	checkpointNext("Two lists with interrupts suspended:");
	testQueueWhileSuspended(cbid);

	checkpointNext("sceGeBreak(1) with interrupts suspended:");
	testBreakWhileSuspended(cbid);

	checkpointNext("Dispatch suspended:");
	testDispatchSuspended(cbid);

	sceGeUnsetCallback(cbid);
	return 0;
}
