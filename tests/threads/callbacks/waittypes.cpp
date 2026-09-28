#include "shared.h"

// A callback runs while a thread waits on each kind of object. What happens to the wait if the
// object is released, deleted or canceled during the callback, or later, or never?

extern "C" {
SceUID sceKernelCreateMutex(const char *name, uint attributes, int initial_count, void *options);
int sceKernelDeleteMutex(SceUID mutexId);
int sceKernelLockMutex(SceUID mutexId, int count, SceUInt *timeout);
int sceKernelLockMutexCB(SceUID mutexId, int count, SceUInt *timeout);
int sceKernelUnlockMutex(SceUID mutexId, int count);
int sceKernelCancelMutex(SceUID mutexId, int count, int *numWaitingThreads);
int sceKernelCancelSema(SceUID semaid, int count, int *numWaitThreads);
int sceKernelCancelEventFlag(SceUID evid, u32 pattern, int *numWaitThreads);
}

enum Type {
	SEMA,
	FLAG,
	MUTEX,
	MBX,
	MSGPIPE_RECV,
	MSGPIPE_SEND,
	VPL,
	FPL,
	TYPE_COUNT,
};

static const char *typeNames[] = { "sema", "event flag", "mutex", "mbx", "msgpipe receive", "msgpipe send", "vpl", "fpl" };

enum Action {
	RELEASE,
	DELETE,
	CANCEL,
	NOTHING,
};

static Type type;
static SceUID obj;
static void *held[32];
static int heldCount;
static SceKernelMsgPacket packet;
static char pipeData[16];

static void create() {
	heldCount = 0;
	switch (type) {
	case SEMA: obj = sceKernelCreateSema("sema", 0, 0, 1, NULL); break;
	case FLAG: obj = sceKernelCreateEventFlag("flag", 0, 0, NULL); break;
	case MUTEX: obj = sceKernelCreateMutex("mutex", 0, 0, NULL); break;
	case MBX: obj = sceKernelCreateMbx("mbx", 0, NULL); break;
	case MSGPIPE_RECV: obj = sceKernelCreateMsgPipe("msgpipe", 2, 0, (void *)16, NULL); break;
	case MSGPIPE_SEND: obj = sceKernelCreateMsgPipe("msgpipe", 2, 0, (void *)16, NULL); break;
	case VPL: obj = sceKernelCreateVpl("vpl", 2, 0, 0x200, NULL); break;
	case FPL: obj = sceKernelCreateFpl("fpl", 2, 0, 0x10, 1, NULL); break;
	default: break;
	}
}

// Runs on the releaser, so it owns what it takes.
static void makeUnavailable() {
	switch (type) {
	case MUTEX:
		sceKernelLockMutex(obj, 1, NULL);
		break;
	case MSGPIPE_SEND:
		sceKernelTrySendMsgPipe(obj, pipeData, 16, 0, NULL);
		break;
	case VPL:
		while (heldCount < 32 && sceKernelTryAllocateVpl(obj, 0x20, &held[heldCount]) == 0) {
			heldCount++;
		}
		break;
	case FPL:
		if (sceKernelTryAllocateFpl(obj, &held[0]) == 0) {
			heldCount = 1;
		}
		break;
	default:
		break;
	}
}

static void release() {
	switch (type) {
	case SEMA: sceKernelSignalSema(obj, 1); break;
	case FLAG: sceKernelSetEventFlag(obj, 1); break;
	case MUTEX: sceKernelUnlockMutex(obj, 1); break;
	case MBX: sceKernelSendMbx(obj, &packet); break;
	case MSGPIPE_RECV: sceKernelSendMsgPipe(obj, pipeData, 4, 0, NULL, NULL); break;
	case MSGPIPE_SEND: sceKernelTryReceiveMsgPipe(obj, pipeData, 16, 0, NULL); break;
	case VPL: sceKernelFreeVpl(obj, held[--heldCount]); break;
	case FPL: sceKernelFreeFpl(obj, held[--heldCount]); break;
	default: break;
	}
}

static void destroy() {
	switch (type) {
	case SEMA: sceKernelDeleteSema(obj); break;
	case FLAG: sceKernelDeleteEventFlag(obj); break;
	case MUTEX: sceKernelDeleteMutex(obj); break;
	case MBX: sceKernelDeleteMbx(obj); break;
	case MSGPIPE_RECV:
	case MSGPIPE_SEND: sceKernelDeleteMsgPipe(obj); break;
	case VPL: sceKernelDeleteVpl(obj); break;
	case FPL: sceKernelDeleteFpl(obj); break;
	default: break;
	}
	obj = 0;
}

static void cancel() {
	switch (type) {
	case SEMA: sceKernelCancelSema(obj, 0, NULL); break;
	case FLAG: sceKernelCancelEventFlag(obj, 0, NULL); break;
	case MUTEX: sceKernelCancelMutex(obj, 1, NULL); break;
	case MBX: sceKernelCancelReceiveMbx(obj, NULL); break;
	case MSGPIPE_RECV:
	case MSGPIPE_SEND: sceKernelCancelMsgPipe(obj, NULL, NULL); break;
	case VPL: sceKernelCancelVpl(obj, NULL); break;
	case FPL: sceKernelCancelFpl(obj, NULL); break;
	default: break;
	}
}

static int wait(SceUInt *timeout) {
	void *data;
	switch (type) {
	case SEMA: return sceKernelWaitSemaCB(obj, 1, timeout);
	case FLAG: return sceKernelWaitEventFlagCB(obj, 1, PSP_EVENT_WAITOR | PSP_EVENT_WAITCLEAR, NULL, timeout);
	case MUTEX: return sceKernelLockMutexCB(obj, 1, timeout);
	case MBX: return sceKernelReceiveMbxCB(obj, &data, timeout);
	case MSGPIPE_RECV: return sceKernelReceiveMsgPipeCB(obj, pipeData, 4, 0, NULL, timeout);
	case MSGPIPE_SEND: return sceKernelSendMsgPipeCB(obj, pipeData, 4, 0, NULL, timeout);
	case VPL: return sceKernelAllocateVplCB(obj, 0x20, &data, timeout);
	case FPL: return sceKernelAllocateFplCB(obj, &data, timeout);
	default: return -1;
	}
}

static volatile Action releaserAction;
static volatile int releaserDelay;
static SceUID releaser;

static int releaserFunc(SceSize argc, void *argv) {
	makeUnavailable();
	if (releaserDelay != 0) {
		sceKernelDelayThread(releaserDelay);
	} else {
		sceKernelSleepThread();
	}
	switch (releaserAction) {
	case RELEASE: release(); break;
	case DELETE: destroy(); break;
	case CANCEL: cancel(); break;
	case NOTHING: break;
	}
	// Keep owning what we hold (e.g. a locked mutex) until told to go.
	sceKernelSleepThread();
	return 0;
}

static int cbFunc(int arg1, int arg2, void *arg) {
	if (arg2 == 1) {
		schedf("    * callback, waking releaser\n");
		sceKernelWakeupThread(releaser);
	} else {
		schedf("    * callback\n");
	}
	return 0;
}

static void runCase(SceUID cb, const char *title, Action action, bool inCallback, bool withTimeout) {
	create();
	releaserAction = action;
	releaserDelay = inCallback ? 0 : 2000;
	releaser = sceKernelCreateThread("releaser", &releaserFunc, 0x10, 0x1000, 0, NULL);
	sceKernelStartThread(releaser, 0, NULL);

	sceKernelNotifyCallback(cb, inCallback ? 1 : 2);
	SceUInt timeout = 5000;
	int result = wait(withTimeout ? &timeout : NULL);
	if (withTimeout) {
		schedf("  %s: %08x (timeout left %s)\n", title, result, timeout == 0 ? "0" : "some");
	} else {
		schedf("  %s: %08x\n", title, result);
	}

	sceKernelWakeupThread(releaser);
	sceKernelDelayThread(1000);
	sceKernelTerminateDeleteThread(releaser);
	if (obj > 0) {
		if (type == MUTEX && result == 0) {
			sceKernelUnlockMutex(obj, 1);
		}
		destroy();
	}
}

extern "C" int main(int argc, char *argv[]) {
	SceUID cb = sceKernelCreateCallback("waittypes", &cbFunc, NULL);

	for (int t = 0; t < TYPE_COUNT; ++t) {
		type = (Type)t;
		checkpointNext(typeNames[t]);
		runCase(cb, "released in callback", RELEASE, true, false);
		runCase(cb, "deleted in callback", DELETE, true, false);
		runCase(cb, "canceled in callback, with timeout", CANCEL, true, true);
		runCase(cb, "canceled after callback, with timeout", CANCEL, false, true);
		runCase(cb, "released after callback", RELEASE, false, false);
		runCase(cb, "timed out", NOTHING, true, true);
		runCase(cb, "released in callback, with timeout", RELEASE, true, true);
		flushschedf();
	}

	sceKernelDeleteCallback(cb);
	return 0;
}
