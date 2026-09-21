#include <common.h>
#include <pspge.h>
#include <pspthreadman.h>
#include <psputils.h>

#include "../commands/commands.h"

extern "C" {
#include "sysmem-imports.h"
}

// Queue bookkeeping details, as read out of ge.prx:
//  - duplicate list detection compares against the list's *start* address, not where it is now
//  - the stack-in-use check only looks at lists that have started executing
//  - between a PAUSE signal and the FINISH that delivers it, the list already reads as paused,
//    and can be neither continued, broken, nor unstalled
//  - a finish callback runs before the next list is started, with its own list still linked

struct PspGeListArgs2 {
	unsigned int size;
	PspGeContext *context;
	u32 numStacks;
	SceGeStack *stacks;
};

static const int LIST_WORDS = 32;
static u32 __attribute__((aligned(16))) lists[8][LIST_WORDS];
static SceGeStack __attribute__((aligned(16))) stack1[4];
static SceGeStack __attribute__((aligned(16))) stack2[4];
static PspGeContext __attribute__((aligned(16))) ctx;

static void resetLists() {
	for (int i = 0; i < 8; ++i) {
		for (int j = 0; j < LIST_WORDS; ++j) {
			lists[i][j] = GE_CMD_NOP << 24;
		}
		lists[i][16] = (GE_CMD_FINISH << 24) | (i + 1);
		lists[i][17] = GE_CMD_END << 24;
	}
	sceKernelDcacheWritebackRange(lists, sizeof(lists));
}

static const char *idResult(int id) {
	static char temp[32];
	if (id >= 0) {
		return "OK";
	}
	snprintf(temp, sizeof(temp), "%08x", id);
	return temp;
}

// Hardware needs a moment to get there, the emulator doesn't.
static void waitForStall(int id) {
	for (int i = 0; i < 100; ++i) {
		if (sceGeListSync(id, 1) == PSP_GE_LIST_STALL_REACHED) {
			return;
		}
		sceKernelDelayThread(1000);
	}
}

static void initStackArgs(PspGeListArgs2 *args, SceGeStack *stack) {
	memset(args, 0, sizeof(*args));
	args->size = sizeof(*args);
	args->numStacks = 4;
	args->stacks = stack;
}

static void testDupAddress(u32 ver) {
	sceKernelSetCompiledSdkVersion(ver);
	resetLists();

	int first = sceGeListEnQueue(lists[0], lists[0] + 2, -1, NULL);
	waitForStall(first);
	checkpoint("  First, now stalled two words in: %s", idResult(first));
	checkpoint("  Same start address: %s", idResult(sceGeListEnQueue(lists[0], lists[0] + 2, -1, NULL)));
	checkpoint("  Same start address, uncached mirror: %s", idResult(sceGeListEnQueue((void *)((u32)lists[0] | 0x40000000), 0, -1, NULL)));
	checkpoint("  Address the first is stalled at: %s", idResult(sceGeListEnQueue(lists[0] + 2, lists[0] + 2, -1, NULL)));
	sceGeBreak(1, NULL);

	first = sceGeListEnQueue(lists[0], lists[0] + 2, -1, NULL);
	waitForStall(first);
	checkpoint("  Break: %s", idResult(sceGeBreak(0, NULL)));
	checkpoint("  After break, start address: %s", idResult(sceGeListEnQueue(lists[0], lists[0], -1, NULL)));
	checkpoint("  After break, address it stopped at: %s", idResult(sceGeListEnQueue(lists[0] + 2, lists[0] + 2, -1, NULL)));
	sceGeBreak(1, NULL);
}

static void testSharedStack(u32 ver) {
	sceKernelSetCompiledSdkVersion(ver);
	resetLists();

	PspGeListArgs2 args1, args2;
	initStackArgs(&args1, stack1);
	initStackArgs(&args2, stack2);

	int running = sceGeListEnQueue(lists[0], lists[0], -1, (PspGeListArgs *)&args1);
	checkpoint("  Running, stack 1: %s", idResult(running));
	checkpoint("  Queued, stack 2: %s", idResult(sceGeListEnQueue(lists[1], lists[1], -1, (PspGeListArgs *)&args2)));
	checkpoint("  Queued, stack 2 again: %s", idResult(sceGeListEnQueue(lists[2], lists[2], -1, (PspGeListArgs *)&args2)));
	checkpoint("  Queued, stack 1 again: %s", idResult(sceGeListEnQueue(lists[3], lists[3], -1, (PspGeListArgs *)&args1)));
	sceGeBreak(1, NULL);
}

static void testDequeueCompleted() {
	resetLists();

	PspGeListArgs2 args;
	memset(&args, 0, sizeof(args));
	args.size = sizeof(args);
	args.context = &ctx;

	int id = sceGeListEnQueue(lists[0], 0, -1, (PspGeListArgs *)&args);
	sceGeListSync(id, 0);
	checkpoint("  Completed, had context: %08x", sceGeListDeQueue(id));
	checkpoint("  Again: %08x", sceGeListDeQueue(id));
	sceGeDrawSync(0);
	checkpoint("  After drawsync: %08x", sceGeListDeQueue(id));
	sceGeBreak(1, NULL);
}

static int pauseListID;

static int pauseSignal(int value, void *arg, u32 *listpc) {
	checkpoint("  * signal %d, listsync: %08x, drawsync: %08x", value, sceGeListSync(pauseListID, 1), sceGeDrawSync(1));
	return 0;
}

static int pauseFinish(int value, void *arg, u32 *listpc) {
	checkpoint("  * finish %d, listsync: %08x, drawsync: %08x", value, sceGeListSync(pauseListID, 1), sceGeDrawSync(1));
	return 0;
}

static void testPauseWindow(u32 ver) {
	sceKernelSetCompiledSdkVersion(ver);
	resetLists();

	u32 *list = lists[0];
	list[1] = (GE_CMD_SIGNAL << 24) | (PSP_GE_SIGNAL_HANDLER_PAUSE << 16) | 0x1234;
	list[2] = GE_CMD_END << 24;
	// Stalls at 4, so the pause has been requested but the FINISH hasn't delivered it.
	list[6] = (GE_CMD_FINISH << 24) | 0x11;
	list[7] = GE_CMD_END << 24;
	sceKernelDcacheWritebackRange(lists, sizeof(lists));

	PspGeCallbackData cbdata;
	cbdata.signal_func = (PspGeCallback)pauseSignal;
	cbdata.signal_arg = NULL;
	cbdata.finish_func = (PspGeCallback)pauseFinish;
	cbdata.finish_arg = NULL;
	int cbid = sceGeSetCallback(&cbdata);

	pauseListID = sceGeListEnQueue(list, list + 4, cbid, NULL);
	sceKernelDelayThread(10000);
	checkpoint("  Pause requested, listsync: %08x, drawsync: %08x", sceGeListSync(pauseListID, 1), sceGeDrawSync(1));
	checkpoint("  Continue: %08x", sceGeContinue());
	checkpoint("  Break: %s", idResult(sceGeBreak(0, NULL)));
	// Only a running list's stall address makes it to the hardware, so this one is now stuck.
	checkpoint("  Update stall: %08x", sceGeListUpdateStallAddr(pauseListID, 0));
	sceKernelDelayThread(10000);
	checkpoint("  After update stall, listsync: %08x, drawsync: %08x", sceGeListSync(pauseListID, 1), sceGeDrawSync(1));
	checkpoint("  Break: %s", idResult(sceGeBreak(0, NULL)));
	checkpoint("  Continue: %08x", sceGeContinue());
	sceKernelDelayThread(10000);
	checkpoint("  After continue, listsync: %08x, drawsync: %08x", sceGeListSync(pauseListID, 1), sceGeDrawSync(1));

	sceGeBreak(1, NULL);
	sceGeUnsetCallback(cbid);
}

static int orderListIDs[2];
static bool orderTriedEnqueue;

static int orderFinish(int value, void *arg, u32 *listpc) {
	checkpoint("  * finish %d, listsync first: %08x, second: %08x, drawsync: %08x", value, sceGeListSync(orderListIDs[0], 1), sceGeListSync(orderListIDs[1], 1), sceGeDrawSync(1));
	if (value == 1 && !orderTriedEnqueue) {
		orderTriedEnqueue = true;
		// Our list is complete, but hasn't been taken off the queue yet.
		int id = sceGeListEnQueue(lists[0], lists[0], -1, NULL);
		checkpoint("  * enqueue own address again: %s", idResult(id));
		if (id >= 0) {
			checkpoint("  * dequeue that: %08x", sceGeListDeQueue(id));
		}
		checkpoint("  * dequeue self: %08x", sceGeListDeQueue(orderListIDs[0]));
		checkpoint("  * update own stall: %08x", sceGeListUpdateStallAddr(orderListIDs[0], 0));
	}
	return 0;
}

static void testFinishOrder(u32 ver) {
	sceKernelSetCompiledSdkVersion(ver);
	resetLists();

	PspGeCallbackData cbdata;
	cbdata.signal_func = NULL;
	cbdata.signal_arg = NULL;
	cbdata.finish_func = (PspGeCallback)orderFinish;
	cbdata.finish_arg = NULL;
	int cbid = sceGeSetCallback(&cbdata);

	orderTriedEnqueue = false;
	orderListIDs[0] = sceGeListEnQueue(lists[0], lists[0] + 1, cbid, NULL);
	orderListIDs[1] = sceGeListEnQueue(lists[1], 0, cbid, NULL);
	checkpoint("  Queued both, listsync first: %08x, second: %08x", sceGeListSync(orderListIDs[0], 1), sceGeListSync(orderListIDs[1], 1));
	// Give the callbacks time to happen, so the order of the output doesn't depend on GE speed.
	int result = sceGeListUpdateStallAddr(orderListIDs[0], 0);
	sceKernelDelayThread(10000);
	checkpoint("  Update stall: %08x", result);
	checkpoint("  Drawsync: %08x", sceGeDrawSync(0));
	checkpoint("  After, listsync first: %08x, second: %08x", sceGeListSync(orderListIDs[0], 1), sceGeListSync(orderListIDs[1], 1));

	sceGeBreak(1, NULL);
	sceGeUnsetCallback(cbid);
}

extern "C" int main(int argc, char *argv[]) {
	checkpointNext("Duplicate address, old SDK:");
	testDupAddress(0);
	checkpointNext("Duplicate address, new SDK:");
	testDupAddress(0x06060010);

	checkpointNext("Shared stack, old SDK:");
	testSharedStack(0);
	checkpointNext("Shared stack, new SDK:");
	testSharedStack(0x06060010);

	checkpointNext("Dequeue completed:");
	testDequeueCompleted();

	checkpointNext("Pause window, old SDK:");
	testPauseWindow(0x01000010);
	checkpointNext("Pause window, new SDK:");
	testPauseWindow(0x06060010);

	checkpointNext("Finish callback ordering, old SDK:");
	testFinishOrder(0x01000010);
	checkpointNext("Finish callback ordering, new SDK:");
	testFinishOrder(0x06060010);

	return 0;
}
