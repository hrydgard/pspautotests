#include <common.h>
#include <pspge.h>
#include <pspthreadman.h>
#include <psputils.h>

#include "../commands/commands.h"

extern "C" {
#include "sysmem-imports.h"
}

// What a GE callback sees: the state the list left, before its context is restored, and a GE that
// has stopped - which is all sceGeSaveContext cares about, queued lists or not.
// Also two things about lists that are done: dequeueing another list doesn't touch them, and
// sceGeBreak(1) has nothing to break once the last one is.

struct PspGeListArgs2 {
	unsigned int size;
	PspGeContext *context;
	u32 numStacks;
	SceGeStack *stacks;
};

static u32 __attribute__((aligned(16))) listBase[16];
static u32 __attribute__((aligned(16))) listA[32];
static u32 __attribute__((aligned(16))) listB[16];
static PspGeContext __attribute__((aligned(16))) listContext;
static PspGeContext __attribute__((aligned(16))) scratchContext;

static int listIDs[2];
static volatile int finishCount;
static bool quietFinish;

static u32 ambient() {
	return sceGeGetCmd(GE_CMD_AMBIENTCOLOR) & 0xFF;
}

static int signalCallback(int value, void *arg, u32 *listpc) {
	// No list state for the CONTINUE signal: the GE is already off again, and how far it has got by
	// now is what gpu/signals/continue is about.
	if (value == 0x21) {
		checkpoint("  * signal %x: ambient %x, sceGeSaveContext: %08x, listsync: %08x", value, ambient(), sceGeSaveContext(&scratchContext), sceGeListSync(listIDs[0], 1));
	} else {
		checkpoint("  * signal %x: ambient %x, sceGeSaveContext: %08x", value, ambient(), sceGeSaveContext(&scratchContext));
	}
	return 0;
}

static int finishCallback(int value, void *arg, u32 *listpc) {
	finishCount++;
	if (quietFinish) {
		// This one can arrive before sceGeListEnQueue has returned the id we'd be asking about.
		return 0;
	}
	checkpoint("  * finish %x: ambient %x, sceGeSaveContext: %08x, first: %08x, second: %08x", value, ambient(), sceGeSaveContext(&scratchContext), sceGeListSync(listIDs[0], 1), sceGeListSync(listIDs[1], 1));
	return 0;
}

static void fillLists() {
	for (int i = 0; i < 16; ++i) {
		listBase[i] = GE_CMD_NOP << 24;
		listB[i] = GE_CMD_NOP << 24;
	}
	for (int i = 0; i < 32; ++i) {
		listA[i] = GE_CMD_NOP << 24;
	}

	listBase[0] = (GE_CMD_AMBIENTCOLOR << 24) | 1;
	listBase[1] = GE_CMD_FINISH << 24;
	listBase[2] = GE_CMD_END << 24;

	listA[0] = (GE_CMD_AMBIENTCOLOR << 24) | 5;
	listA[1] = (GE_CMD_SIGNAL << 24) | (PSP_GE_SIGNAL_HANDLER_SUSPEND << 16) | 0x21;
	listA[2] = GE_CMD_END << 24;
	listA[3] = (GE_CMD_AMBIENTCOLOR << 24) | 6;
	listA[4] = (GE_CMD_SIGNAL << 24) | (PSP_GE_SIGNAL_HANDLER_CONTINUE << 16) | 0x22;
	listA[5] = GE_CMD_END << 24;
	// Stalls at 7, so the GE is running again, but not going anywhere, during the second callback.
	listA[7] = (GE_CMD_AMBIENTCOLOR << 24) | 7;
	listA[8] = (GE_CMD_FINISH << 24) | 1;
	listA[9] = GE_CMD_END << 24;

	listB[4] = (GE_CMD_FINISH << 24) | 2;
	listB[5] = GE_CMD_END << 24;

	sceKernelDcacheWritebackRange(listBase, sizeof(listBase));
	sceKernelDcacheWritebackRange(listA, sizeof(listA));
	sceKernelDcacheWritebackRange(listB, sizeof(listB));
}

static void testCallbackState(int cbid) {
	int id = sceGeListEnQueue(listBase, 0, -1, NULL);
	sceGeListSync(id, 0);
	sceGeDrawSync(0);
	checkpoint("  Before: ambient %x, sceGeSaveContext: %08x", ambient(), sceGeSaveContext(&scratchContext));

	PspGeListArgs2 args;
	memset(&args, 0, sizeof(args));
	args.size = sizeof(args);
	args.context = &listContext;

	// Stalled at the start, so we have both ids before anything happens.
	listIDs[0] = sceGeListEnQueue(listA, listA, cbid, (PspGeListArgs *)&args);
	listIDs[1] = sceGeListEnQueue(listB, 0, cbid, NULL);
	sceGeListUpdateStallAddr(listIDs[0], listA + 7);
	sceKernelDelayThread(10000);
	checkpoint("  Stalled: ambient %x, sceGeSaveContext: %08x, listsync: %08x", ambient(), sceGeSaveContext(&scratchContext), sceGeListSync(listIDs[0], 1));

	sceGeListUpdateStallAddr(listIDs[0], 0);
	sceKernelDelayThread(10000);
	checkpoint("  After: ambient %x, sceGeSaveContext: %08x, finished: %d", ambient(), sceGeSaveContext(&scratchContext), finishCount);
	sceGeDrawSync(0);
	sceGeBreak(1, NULL);
}

static void testDequeueLeavesCompleted() {
	int done = sceGeListEnQueue(listBase, 0, -1, NULL);
	sceGeListSync(done, 0);
	checkpoint("  Completed list: %08x", sceGeListSync(done, 1));

	// At the head of an empty queue a list is paused, and never started, so it can be dequeued.
	int head = sceGeListEnQueueHead(listB, 0, -1, NULL);
	checkpoint("  Head: %08x, completed list: %08x", sceGeListSync(head, 1), sceGeListSync(done, 1));
	checkpoint("  Dequeue head: %08x", sceGeListDeQueue(head));
	checkpoint("  Queue empty again, completed list: %08x, head: %08x, drawsync: %08x", sceGeListSync(done, 1), sceGeListSync(head, 1), sceGeDrawSync(1));
	sceGeDrawSync(0);
	checkpoint("  After drawsync, completed list: %08x", sceGeListSync(done, 1));
}

static void testBreakWhenDone(int cbid) {
	finishCount = 0;
	quietFinish = true;
	listIDs[0] = sceGeListEnQueue(listB, 0, cbid, NULL);
	// No waiting: a list this short is over and done with before sceGeListEnQueue even returns.
	int result = sceGeBreak(1, NULL);
	sceKernelDelayThread(10000);
	checkpoint("  sceGeBreak(1) right after a short list: %08x, finished: %d, list: %08x", result, finishCount, sceGeListSync(listIDs[0], 1));
	sceGeDrawSync(0);
}

extern "C" int main(int argc, char *argv[]) {
	sceKernelSetCompiledSdkVersion(0x06060010);
	fillLists();

	PspGeCallbackData cbdata;
	cbdata.signal_func = (PspGeCallback)signalCallback;
	cbdata.signal_arg = NULL;
	cbdata.finish_func = (PspGeCallback)finishCallback;
	cbdata.finish_arg = NULL;
	int cbid = sceGeSetCallback(&cbdata);

	checkpointNext("State seen from callbacks:");
	testCallbackState(cbid);

	checkpointNext("Dequeue and completed lists:");
	testDequeueLeavesCompleted();

	checkpointNext("Break with nothing left:");
	testBreakWhenDone(cbid);

	sceGeUnsetCallback(cbid);
	return 0;
}
