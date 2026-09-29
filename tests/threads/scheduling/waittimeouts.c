#include <common.h>
#include <pspthreadman.h>
#include <pspmodulemgr.h>

// How long a wait that times out really takes, for every kind of wait with a timeout, the shortest
// of several tries. A very short one times out at once without writing the timeout back; how short
// depends on the call, and around the edge it goes either way from run to run, so that isn't tested.
// Anything longer ends max(timeout, 205us) + about 35us after the call: "on time" is within 10us
// of that.

int sceKernelCreateMutex(const char *name, u32 attr, int count, void *options);
int sceKernelLockMutex(SceUID uid, int count, SceUInt *timeout);
int sceKernelUnlockMutex(SceUID uid, int count);
int sceKernelLockMutexCB(SceUID uid, int count, SceUInt *timeout);
int sceKernelLockLwMutexCB(SceLwMutexWorkarea *workarea, int count, SceUInt *timeout);

static SceUID sema, flag, mutex, mbx, pipe, fpl, vpl, holderThread;
static SceLwMutexWorkarea lwmutex;
static void *fplBlock, *vplBlock;
static volatile int holderStop = 0;

static int holder(SceSize argc, void *argp) {
	sceKernelLockMutex(mutex, 1, NULL);
	sceKernelLockLwMutex(&lwmutex, 1, NULL);
	while (!holderStop) {
		sceKernelDelayThread(10000);
	}
	sceKernelUnlockLwMutex(&lwmutex, 1);
	sceKernelUnlockMutex(mutex, 1);
	return 0;
}

static int endless(SceSize argc, void *argp) {
	sceKernelSleepThread();
	return 0;
}

static int waitWith(int type, SceUInt *timeout) {
	char buf[16];
	void *ptr;
	switch (type) {
	case 0: return sceKernelWaitSema(sema, 1, timeout);
	case 1: return sceKernelWaitSemaCB(sema, 1, timeout);
	case 2: return sceKernelWaitEventFlag(flag, 1, PSP_EVENT_WAITOR, NULL, timeout);
	case 3: return sceKernelLockMutex(mutex, 1, timeout);
	case 4: return sceKernelLockLwMutex(&lwmutex, 1, timeout);
	case 5: return sceKernelReceiveMbx(mbx, &ptr, timeout);
	case 6: return sceKernelReceiveMsgPipe(pipe, buf, sizeof(buf), 0, NULL, timeout);
	case 7: return sceKernelAllocateFpl(fpl, &ptr, timeout);
	case 8: return sceKernelAllocateVpl(vpl, 0x100, &ptr, timeout);
	case 9: return sceKernelWaitThreadEnd(holderThread, timeout);
	case 10: return sceKernelWaitEventFlagCB(flag, 1, PSP_EVENT_WAITOR, NULL, timeout);
	case 11: return sceKernelLockMutexCB(mutex, 1, timeout);
	case 12: return sceKernelLockLwMutexCB(&lwmutex, 1, timeout);
	case 13: return sceKernelReceiveMbxCB(mbx, &ptr, timeout);
	case 14: return sceKernelReceiveMsgPipeCB(pipe, buf, sizeof(buf), 0, NULL, timeout);
	case 15: return sceKernelAllocateFplCB(fpl, &ptr, timeout);
	case 16: return sceKernelAllocateVplCB(vpl, 0x100, &ptr, timeout);
	case 17: return sceKernelWaitThreadEndCB(holderThread, timeout);
	}
	return 0;
}

static const char *names[] = {
	"WaitSema", "WaitSemaCB", "WaitEventFlag", "LockMutex", "LockLwMutex", "ReceiveMbx",
	"ReceiveMsgPipe", "AllocateFpl", "AllocateVpl", "WaitThreadEnd",
	"WaitEventFlagCB", "LockMutexCB", "LockLwMutexCB", "ReceiveMbxCB", "ReceiveMsgPipeCB",
	"AllocateFplCB", "AllocateVplCB", "WaitThreadEndCB",
};

int main(int argc, char *argv[]) {
	sema = sceKernelCreateSema("sema", 0, 0, 1, NULL);
	flag = sceKernelCreateEventFlag("flag", 0, 0, NULL);
	mutex = sceKernelCreateMutex("mutex", 0, 0, NULL);
	sceKernelCreateLwMutex(&lwmutex, "lwmutex", 0, 0, NULL);
	mbx = sceKernelCreateMbx("mbx", 0, NULL);
	pipe = sceKernelCreateMsgPipe("pipe", PSP_MEMORY_PARTITION_USER, 0, (void *)0x100, NULL);
	fpl = sceKernelCreateFpl("fpl", PSP_MEMORY_PARTITION_USER, 0, 0x100, 1, NULL);
	sceKernelAllocateFpl(fpl, &fplBlock, NULL);
	vpl = sceKernelCreateVpl("vpl", PSP_MEMORY_PARTITION_USER, 0, 0x200, NULL);
	sceKernelAllocateVpl(vpl, 0x100, &vplBlock, NULL);

	holderThread = sceKernelCreateThread("holder", &holder, 0x10, 0x1000, 0, NULL);
	sceKernelStartThread(holderThread, 0, NULL);

	static const u32 timeouts[] = { 0, 1, 100, 205, 210, 220, 1000 };

	checkpointNext("How each timeout went, and what's left in it afterwards:");
	for (int type = 0; type < (int)(sizeof(names) / sizeof(names[0])); ++type) {
		char line[512];
		int pos = sprintf(line, "  %s:", names[type]);
		int result = 0;
		for (int i = 0; i < (int)(sizeof(timeouts) / sizeof(timeouts[0])); ++i) {
			u32 best = 0xFFFFFFFF;
			SceUInt left = 0;
			for (int k = 0; k < 4; ++k) {
				SceUInt timeout = timeouts[i];
				u32 start = sceKernelGetSystemTimeLow();
				result = waitWith(type, &timeout);
				u32 t = sceKernelGetSystemTimeLow() - start;
				if (t < best) {
					best = t;
					left = timeout;
				}
			}
			u32 expected = (timeouts[i] > 205 ? timeouts[i] : 205) + 35;
			if (best < 50) {
				pos += sprintf(line + pos, " %d=at once", (int)timeouts[i]);
			} else if (best + 10 >= expected && best <= expected + 10) {
				pos += sprintf(line + pos, " %d=on time", (int)timeouts[i]);
			} else {
				pos += sprintf(line + pos, " %d=%dus", (int)timeouts[i], (int)best);
			}
			if (left != 0) {
				pos += sprintf(line + pos, "(left %d)", (int)left);
			}
		}
		schedf("%s, result %08x\n", line, result);
	}

	holderStop = 1;
	sceKernelWaitThreadEnd(holderThread, NULL);
	sceKernelDeleteThread(holderThread);
	flushschedf();
	return 0;
}
