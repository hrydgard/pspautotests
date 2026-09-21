#include <common.h>
#include <pspge.h>
#include <pspthreadman.h>
#include <psputils.h>

#include "../commands/commands.h"

// What happens to threads waiting in sceGeListSync / sceGeDrawSync when sceGeBreak(1) throws
// the whole queue away underneath them.

static u32 __attribute__((aligned(16))) lists[2][32];

static int waitListID;
static SceUID listThread;
static SceUID drawThread;

static int listWaiter(SceSize argc, void *argv) {
	int result = sceGeListSync(waitListID, 0);
	checkpoint("  * sceGeListSync woke: %08x", result);
	return 0;
}

static int drawWaiter(SceSize argc, void *argv) {
	int result = sceGeDrawSync(0);
	checkpoint("  * sceGeDrawSync woke: %08x", result);
	return 0;
}

static const char *threadState(SceUID thread) {
	SceKernelThreadInfo info;
	info.size = sizeof(info);
	if (sceKernelReferThreadStatus(thread, &info) < 0) {
		return "gone";
	}
	if (info.status & PSP_THREAD_WAITING) {
		return "waiting";
	}
	if (info.status & (PSP_THREAD_STOPPED | PSP_THREAD_KILLED)) {
		return "ended";
	}
	return "other";
}

static void logThreads(const char *title) {
	checkpoint("  %s: list waiter %s, draw waiter %s", title, threadState(listThread), threadState(drawThread));
}

extern "C" int main(int argc, char *argv[]) {
	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < 32; ++j) {
			lists[i][j] = GE_CMD_NOP << 24;
		}
		lists[i][8] = GE_CMD_FINISH << 24;
		lists[i][9] = GE_CMD_END << 24;
	}
	sceKernelDcacheWritebackRange(lists, sizeof(lists));

	checkpointNext("Break with waiting threads:");
	// Stalled right at the start, so it never completes by itself.
	waitListID = sceGeListEnQueue(lists[0], lists[0], -1, NULL);
	checkpoint("  Enqueue stalled: %s", waitListID >= 0 ? "OK" : "failed");

	listThread = sceKernelCreateThread("listWaiter", &listWaiter, 0x10, 0x1000, 0, NULL);
	drawThread = sceKernelCreateThread("drawWaiter", &drawWaiter, 0x10, 0x1000, 0, NULL);
	sceKernelStartThread(listThread, 0, NULL);
	sceKernelStartThread(drawThread, 0, NULL);
	logThreads("Before break");

	checkpoint("  sceGeBreak(1): %08x", sceGeBreak(1, NULL));
	sceKernelDelayThread(10000);
	logThreads("After break");
	checkpoint("  Old list, sceGeListSync(1): %08x", sceGeListSync(waitListID, 1));
	checkpoint("  sceGeDrawSync(1): %08x", sceGeDrawSync(1));

	checkpointNext("A new list completes:");
	int newListID = sceGeListEnQueue(lists[1], 0, -1, NULL);
	// Let it complete before logging, so the order of the output doesn't depend on GE speed.
	sceKernelDelayThread(10000);
	checkpoint("  Enqueue: %s, id %s", newListID >= 0 ? "OK" : "failed", newListID == waitListID ? "reused" : "differs");
	logThreads("After new list");
	checkpoint("  sceGeDrawSync(0): %08x", sceGeDrawSync(0));
	logThreads("After drawsync");

	sceKernelTerminateDeleteThread(listThread);
	sceKernelTerminateDeleteThread(drawThread);
	sceGeBreak(1, NULL);
	return 0;
}
