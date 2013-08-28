#include <common.h>

#include <pspthreadman.h>
#include <pspintrman.h>
#include <pspdisplay.h>
#include <pspmodulemgr.h>
#include <stdarg.h>

typedef struct SceKernelTlsOptParam {
	SceSize size;
	u32 alignment;
} SceKernelTlsOptParam;

extern "C" {
SceUID sceKernelCreateTls(const char *name, u32 partitionid, u32 attr, u32 blockSize, u32 count, SceKernelTlsOptParam *options);
int sceKernelDeleteTls(SceUID uid);
int sceKernelAllocateTls(SceUID uid);
int sceKernelFreeTls(SceUID uid);
int sceKernelReferTlsStatus(SceUID uid, void *info);

SceUID sceKernelCreateMutex(const char *name, uint attributes, int initial_count, void *options);
int sceKernelDeleteMutex(SceUID mutexId);
int sceKernelLockMutex(SceUID mutexId, int count, SceUInt *timeout);
int sceKernelLockMutexCB(SceUID mutexId, int count, SceUInt *timeout);
int sceKernelTryLockMutex(SceUID mutexId, int count);
int sceKernelUnlockMutex(SceUID mutexId, int count);

int sceKernelCreateLwMutex(void *workarea, const char *name, uint attr, int count, void *options);
int sceKernelDeleteLwMutex(void *workarea);
int sceKernelTryLockLwMutex(void *workarea, int count);
int sceKernelTryLockLwMutex_600(void *workarea, int count);
int sceKernelLockLwMutex(void *workarea, int count, SceUInt *timeout);
int sceKernelLockLwMutexCB(void *workarea, int count, SceUInt *timeout);
int sceKernelUnlockLwMutex(void *workarea, int count);

int sceDisplayWaitVblankStartMulti(int vblanks);
int sceDisplayWaitVblankStartMultiCB(int vblanks);
}

int sceKernelAllocateTlsHelper(SceUID uid) {
	int result = sceKernelAllocateTls(uid);
	if (result > 0) {
		return 0x1337;
	}
	return result;
}

extern SceUID reschedThread;
extern volatile int didResched;

extern u64 lastCheckpoint;
void safeCheckpoint(const char *format, ...) {
	int state = sceKernelSuspendDispatchThread();
	int result = sceKernelResumeDispatchThread(state);

	const char *reschedState = didResched ? "?" : "x";
	if (didResched == 1) {
		reschedState = "r";
	}
	didResched = 2;

	u64 currentCheckpoint = sceKernelGetSystemTimeWide();
	if (CHECKPOINT_ENABLE_TIME) {
		schedf("[%s/%lld] ", reschedState, currentCheckpoint - lastCheckpoint);
	} else {
		schedf("[%s] ", reschedState);
	}

	if (state == 1) {
		sceKernelTerminateThread(reschedThread);
	}

	if (format != NULL) {
		va_list args;
		va_start(args, format);
		schedfBufferPos += vsprintf(schedfBuffer + schedfBufferPos, format, args);
		// This is easier to debug in the emulator, but printf() reschedules on the real PSP.
		//vprintf(format, args);
		va_end(args);
	}

	if (state == 1) {
		didResched = 0;
		sceKernelStartThread(reschedThread, 0, NULL);
	}

	if (format != NULL) {
		schedf("\n");
	}

	lastCheckpoint = currentCheckpoint;
}

#define INTR_DISPATCH(expr) \
	{ \
		state = sceKernelCpuSuspendIntr(); \
		safeCheckpoint("  Interrupts disabled: %08x", expr); \
		sceKernelCpuResumeIntr(state); \
		state = sceKernelSuspendDispatchThread(); \
		safeCheckpoint("  Dispatch disabled: %08x", expr); \
		sceKernelResumeDispatchThread(state); \
	}

#define INTR_DISPATCH_TITLE(title, expr) \
	{ \
		state = sceKernelCpuSuspendIntr(); \
		safeCheckpoint("  %s - interrupts disabled: %08x", title, expr); \
		sceKernelCpuResumeIntr(state); \
		state = sceKernelSuspendDispatchThread(); \
		safeCheckpoint("  %s - dispatch disabled: %08x", title, expr); \
		sceKernelResumeDispatchThread(state); \
	}

bool intrRan = false;
SceUID intrSema;
SceUID intrFlag;

extern "C" void interruptFunc(int no, void *arg) {
	if (intrRan) {
		return;
	}
	intrRan = true;
	checkpoint("  ** Inside interrupt");
	
	SceKernelSysClock clock = {200};
	safeCheckpoint("  sceKernelDelayThread: %08x", sceKernelDelayThread(200));
	safeCheckpoint("  sceKernelDelayThreadCB: %08x", sceKernelDelayThreadCB(200));
	safeCheckpoint("  sceKernelDelaySysClockThread: %08x", sceKernelDelaySysClockThread(&clock));
	safeCheckpoint("  sceKernelDelaySysClockThreadCB: %08x", sceKernelDelaySysClockThreadCB(&clock));
	safeCheckpoint("  sceKernelSleepThread: %08x", sceKernelSleepThread());
	safeCheckpoint("  sceKernelSleepThreadCB: %08x", sceKernelSleepThreadCB());

	safeCheckpoint("  sceDisplayWaitVblank: %08x", sceDisplayWaitVblank());
	safeCheckpoint("  sceDisplayWaitVblankCB: %08x", sceDisplayWaitVblankCB());
	safeCheckpoint("  sceDisplayWaitVblankStart: %08x", sceDisplayWaitVblankStart());
	safeCheckpoint("  sceDisplayWaitVblankStartCB: %08x", sceDisplayWaitVblankStartCB());
	safeCheckpoint("  sceDisplayWaitVblankStartMulti - invalid count: %08x", sceDisplayWaitVblankStartMulti(0));
	safeCheckpoint("  sceDisplayWaitVblankStartMulti - valid count: %08x", sceDisplayWaitVblankStartMulti(1));
	safeCheckpoint("  sceDisplayWaitVblankStartMultiCB - invalid count: %08x", sceDisplayWaitVblankStartMultiCB(0));
	safeCheckpoint("  sceDisplayWaitVblankStartMultiCB - valid count: %08x", sceDisplayWaitVblankStartMultiCB(1));

	safeCheckpoint("  sceKernelWaitSema - bad sema: %08x", sceKernelWaitSema(0, 1, NULL));
	safeCheckpoint("  sceKernelWaitSema - invalid count: %08x", sceKernelWaitSema(intrSema, 9, NULL));
	safeCheckpoint("  sceKernelWaitSema - valid: %08x", sceKernelWaitSema(intrSema, 1, NULL));
	safeCheckpoint("  sceKernelWaitSemaCB - bad sema: %08x", sceKernelWaitSemaCB(0, 1, NULL));
	safeCheckpoint("  sceKernelWaitSemaCB - invalid count: %08x", sceKernelWaitSemaCB(intrSema, 9, NULL));
	safeCheckpoint("  sceKernelWaitSemaCB - valid: %08x", sceKernelWaitSemaCB(intrSema, 1, NULL));

	safeCheckpoint("  sceKernelWaitEventFlag - bad flag: %08x", sceKernelWaitEventFlag(0, 1, PSP_EVENT_WAITAND, NULL, NULL));
	safeCheckpoint("  sceKernelWaitEventFlag - invalid mode: %08x", sceKernelWaitEventFlag(intrFlag, 1, 0xFF, NULL, NULL));
	safeCheckpoint("  sceKernelWaitEventFlag - valid flag: %08x", sceKernelWaitEventFlag(intrFlag, 1, PSP_EVENT_WAITAND, NULL, NULL));
	safeCheckpoint("  sceKernelWaitEventFlagCB - bad flag: %08x", sceKernelWaitEventFlagCB(0, 1, PSP_EVENT_WAITAND, NULL, NULL));
	safeCheckpoint("  sceKernelWaitEventFlagCB - invalid mode: %08x", sceKernelWaitEventFlagCB(intrFlag, 1, 0xFF, NULL, NULL));
	safeCheckpoint("  sceKernelWaitEventFlagCB - valid flag: %08x", sceKernelWaitEventFlagCB(intrFlag, 1, PSP_EVENT_WAITAND, NULL, NULL));
}

extern "C" int dummyThread(SceSize argc, void *argp) {
	return 0;
}

extern "C" int main(int argc, char *argv[]) {
	int state;
	void *ptr;
	char buf[256];

	SceKernelSysClock clock = {200};
	checkpointNext("sceKernelDelayThread:");
	INTR_DISPATCH(sceKernelDelayThread(200));
	checkpointNext("sceKernelDelayThreadCB:");
	INTR_DISPATCH(sceKernelDelayThreadCB(200));
	checkpointNext("sceKernelDelaySysClockThread:");
	INTR_DISPATCH(sceKernelDelaySysClockThread(&clock));
	checkpointNext("sceKernelDelaySysClockThreadCB:");
	INTR_DISPATCH(sceKernelDelaySysClockThreadCB(&clock));
	checkpointNext("sceKernelSleepThread:");
	INTR_DISPATCH(sceKernelSleepThread());
	checkpointNext("sceKernelSleepThreadCB:");
	INTR_DISPATCH(sceKernelSleepThreadCB());

	checkpointNext("sceDisplayWaitVblank:");
	INTR_DISPATCH(sceDisplayWaitVblank());
	checkpointNext("sceDisplayWaitVblankCB:");
	INTR_DISPATCH(sceDisplayWaitVblankCB());
	checkpointNext("sceDisplayWaitVblankStart:");
	INTR_DISPATCH(sceDisplayWaitVblankStart());
	checkpointNext("sceDisplayWaitVblankStartCB:");
	INTR_DISPATCH(sceDisplayWaitVblankStartCB());
	checkpointNext("sceDisplayWaitVblankStartMulti:");
	INTR_DISPATCH_TITLE("Invalid count", sceDisplayWaitVblankStartMulti(0));
	INTR_DISPATCH_TITLE("Valid count", sceDisplayWaitVblankStartMulti(1));
	checkpointNext("sceDisplayWaitVblankStartMultiCB:");
	INTR_DISPATCH_TITLE("Invalid count", sceDisplayWaitVblankStartMultiCB(0));
	INTR_DISPATCH_TITLE("Valid count", sceDisplayWaitVblankStartMultiCB(1));

	SceUID sema = sceKernelCreateSema("sema", 0, 0, 1, NULL);
	checkpointNext("sceKernelWaitSema:");
	INTR_DISPATCH_TITLE("Bad sema", sceKernelWaitSema(0, 1, NULL));
	INTR_DISPATCH_TITLE("Invalid count", sceKernelWaitSema(sema, 9, NULL));
	INTR_DISPATCH_TITLE("Valid sema", sceKernelWaitSema(sema, 1, NULL));
	checkpointNext("sceKernelWaitSemaCB:");
	INTR_DISPATCH_TITLE("Bad sema", sceKernelWaitSemaCB(0, 1, NULL));
	INTR_DISPATCH_TITLE("Invalid count", sceKernelWaitSemaCB(sema, 9, NULL));
	INTR_DISPATCH_TITLE("Valid sema", sceKernelWaitSemaCB(sema, 1, NULL));
	sceKernelDeleteSema(sema);

	SceUID flag = sceKernelCreateEventFlag("flag", 0, 0, NULL);
	checkpointNext("sceKernelWaitEventFlag:");
	INTR_DISPATCH_TITLE("Bad flag", sceKernelWaitEventFlag(0, 1, PSP_EVENT_WAITAND, NULL, NULL));
	INTR_DISPATCH_TITLE("Invalid mode", sceKernelWaitEventFlag(flag, 1, 0xFF, NULL, NULL));
	INTR_DISPATCH_TITLE("Valid flag", sceKernelWaitEventFlag(flag, 1, PSP_EVENT_WAITAND, NULL, NULL));
	sceKernelSetEventFlag(flag, 1);
	INTR_DISPATCH_TITLE("Already set", sceKernelWaitEventFlag(flag, 1, PSP_EVENT_WAITAND, NULL, NULL));
	sceKernelDeleteEventFlag(flag);
	flag = sceKernelCreateEventFlag("flag", 0, 0, NULL);
	checkpointNext("sceKernelWaitEventFlagCB:");
	INTR_DISPATCH_TITLE("Bad flag", sceKernelWaitEventFlagCB(0, 1, PSP_EVENT_WAITAND, NULL, NULL));
	INTR_DISPATCH_TITLE("Invalid mode", sceKernelWaitEventFlagCB(flag, 1, 0xFF, NULL, NULL));
	INTR_DISPATCH_TITLE("Valid flag", sceKernelWaitEventFlagCB(flag, 1, PSP_EVENT_WAITAND, NULL, NULL));
	sceKernelSetEventFlag(flag, 1);
	INTR_DISPATCH_TITLE("Already set", sceKernelWaitEventFlagCB(flag, 1, PSP_EVENT_WAITAND, NULL, NULL));
	sceKernelDeleteEventFlag(flag);

	// TODO: Need to test below here inside the interrupt.

	SceUID mbx = sceKernelCreateMbx("mbx", 0, NULL);
	checkpointNext("sceKernelReceiveMbx:");
	INTR_DISPATCH_TITLE("Bad mbx", sceKernelReceiveMbx(0, &ptr, NULL));
	INTR_DISPATCH_TITLE("Valid mbx", sceKernelReceiveMbx(mbx, &ptr, NULL));
	checkpointNext("sceKernelReceiveMbxCB:");
	INTR_DISPATCH_TITLE("Bad mbx", sceKernelReceiveMbxCB(0, &ptr, NULL));
	INTR_DISPATCH_TITLE("Valid mbx", sceKernelReceiveMbxCB(mbx, &ptr, NULL));
	sceKernelDeleteMbx(mbx);

	SceUID fpl = sceKernelCreateFpl("fpl", PSP_MEMORY_PARTITION_USER, 0, 0x100, 0x10, NULL);
	checkpointNext("sceKernelAllocateFpl:");
	INTR_DISPATCH_TITLE("Bad fpl", sceKernelAllocateFpl(0, &ptr, NULL));
	INTR_DISPATCH_TITLE("Valid fpl", sceKernelAllocateFpl(fpl, &ptr, NULL));
	checkpointNext("sceKernelAllocateFplCB:");
	INTR_DISPATCH_TITLE("Bad fpl", sceKernelAllocateFplCB(0, &ptr, NULL));
	INTR_DISPATCH_TITLE("Valid fpl", sceKernelAllocateFplCB(fpl, &ptr, NULL));
	sceKernelDeleteFpl(fpl);

	SceUID vpl = sceKernelCreateVpl("vpl", PSP_MEMORY_PARTITION_USER, 0, 0x100, NULL);
	checkpointNext("sceKernelAllocateVpl:");
	INTR_DISPATCH_TITLE("Bad vpl", sceKernelAllocateVpl(0, 0x10, &ptr, NULL));
	INTR_DISPATCH_TITLE("Bad size", sceKernelAllocateVpl(vpl, 0, &ptr, NULL));
	INTR_DISPATCH_TITLE("Valid vpl", sceKernelAllocateVpl(vpl, 0x10, &ptr, NULL));
	checkpointNext("sceKernelAllocateVplCB:");
	INTR_DISPATCH_TITLE("Bad vpl", sceKernelAllocateVplCB(0, 0x10, &ptr, NULL));
	INTR_DISPATCH_TITLE("Bad size", sceKernelAllocateVplCB(vpl, 0, &ptr, NULL));
	INTR_DISPATCH_TITLE("Valid vpl", sceKernelAllocateVplCB(vpl, 0x10, &ptr, NULL));
	sceKernelDeleteVpl(vpl);

	SceUID tls = sceKernelCreateTls("tls", PSP_MEMORY_PARTITION_USER, 0, 0x100, 0x10, NULL);
	checkpointNext("sceKernelAllocateTls:");
	INTR_DISPATCH_TITLE("Bad tls", sceKernelAllocateTlsHelper(0));
	INTR_DISPATCH_TITLE("Valid tls", sceKernelAllocateTlsHelper(tls));
	sceKernelDeleteTls(tls);

	SceUID msgpipe = sceKernelCreateMsgPipe("msgpipe", PSP_MEMORY_PARTITION_USER, 0, (void *)0, NULL);
	checkpointNext("sceKernelReceiveMsgPipe:");
	INTR_DISPATCH_TITLE("Bad msgpipe", sceKernelReceiveMsgPipe(0, buf, 256, 0, NULL, NULL));
	INTR_DISPATCH_TITLE("Bad size", sceKernelReceiveMsgPipe(msgpipe, buf, -1, 0, NULL, NULL));
	INTR_DISPATCH_TITLE("Valid msgpipe", sceKernelReceiveMsgPipe(msgpipe, buf, 256, 0, NULL, NULL));
	checkpointNext("sceKernelReceiveMsgPipeCB:");
	INTR_DISPATCH_TITLE("Bad msgpipe", sceKernelReceiveMsgPipeCB(0, buf, 256, 0, NULL, NULL));
	INTR_DISPATCH_TITLE("Bad size", sceKernelReceiveMsgPipeCB(msgpipe, buf, -1, 0, NULL, NULL));
	INTR_DISPATCH_TITLE("Valid msgpipe", sceKernelReceiveMsgPipeCB(msgpipe, buf, 256, 0, NULL, NULL));
	checkpointNext("sceKernelSendMsgPipe:");
	INTR_DISPATCH_TITLE("Bad msgpipe", sceKernelSendMsgPipe(0, buf, 256, 0, NULL, NULL));
	INTR_DISPATCH_TITLE("Bad size", sceKernelSendMsgPipe(msgpipe, buf, -1, 0, NULL, NULL));
	INTR_DISPATCH_TITLE("Valid msgpipe", sceKernelSendMsgPipe(msgpipe, buf, 256, 0, NULL, NULL));
	checkpointNext("sceKernelSendMsgPipeCB:");
	INTR_DISPATCH_TITLE("Bad msgpipe", sceKernelSendMsgPipeCB(0, buf, 256, 0, NULL, NULL));
	INTR_DISPATCH_TITLE("Bad size", sceKernelSendMsgPipeCB(msgpipe, buf, -1, 0, NULL, NULL));
	INTR_DISPATCH_TITLE("Valid msgpipe", sceKernelSendMsgPipeCB(msgpipe, buf, 256, 0, NULL, NULL));
	sceKernelDeleteMsgPipe(msgpipe);

	SceUID mutex = sceKernelCreateMutex("mutex", 0, 0, NULL);
	checkpointNext("sceKernelLockMutex:");
	INTR_DISPATCH_TITLE("Bad mutex", sceKernelLockMutex(0, 1, NULL));
	INTR_DISPATCH_TITLE("Bad count", sceKernelLockMutex(mutex, 9, NULL));
	INTR_DISPATCH_TITLE("Valid mutex", sceKernelLockMutex(mutex, 1, NULL));
	checkpointNext("sceKernelLockMutexCB:");
	INTR_DISPATCH_TITLE("Bad mutex", sceKernelLockMutexCB(0, 1, NULL));
	INTR_DISPATCH_TITLE("Bad count", sceKernelLockMutexCB(mutex, 9, NULL));
	INTR_DISPATCH_TITLE("Valid mutex", sceKernelLockMutexCB(mutex, 1, NULL));
	sceKernelDeleteMutex(mutex);

	sceKernelCreateLwMutex(buf, "lwmutex", 0, 0, NULL);
	checkpointNext("sceKernelLockLwMutex:");
	INTR_DISPATCH_TITLE("Bad count", sceKernelLockLwMutex(buf, 9, NULL));
	INTR_DISPATCH_TITLE("Valid mutex", sceKernelLockLwMutex(buf, 1, NULL));
	checkpointNext("sceKernelLockLwMutexCB:");
	INTR_DISPATCH_TITLE("Bad count", sceKernelLockLwMutexCB(buf, 9, NULL));
	INTR_DISPATCH_TITLE("Valid mutex", sceKernelLockLwMutexCB(buf, 1, NULL));
	sceKernelDeleteMutex(mutex);

	checkpointNext("sceKernelStartModule:");
	INTR_DISPATCH(sceKernelStartModule(sceKernelGetModuleId(), 0, NULL, NULL, NULL));
	checkpointNext("sceKernelStopModule:");
	INTR_DISPATCH(sceKernelStopModule(sceKernelGetModuleId(), 0, NULL, NULL, NULL));

	SceUID notRunningThread = sceKernelCreateThread("notRunning", &dummyThread, 0x20, 0x1000, 0, NULL);
	checkpointNext("sceKernelWaitThreadEnd:");
	INTR_DISPATCH_TITLE("Bad thread", sceKernelWaitThreadEnd(0, NULL));
	INTR_DISPATCH_TITLE("Not running", sceKernelWaitThreadEnd(notRunningThread, NULL));
	INTR_DISPATCH_TITLE("Running", sceKernelWaitThreadEnd(reschedThread, NULL));
	checkpointNext("sceKernelWaitThreadEndCB:");
	INTR_DISPATCH_TITLE("Bad thread", sceKernelWaitThreadEndCB(0, NULL));
	INTR_DISPATCH_TITLE("Not running", sceKernelWaitThreadEndCB(notRunningThread, NULL));
	INTR_DISPATCH_TITLE("Running", sceKernelWaitThreadEndCB(reschedThread, NULL));
	sceKernelDeleteThread(notRunningThread);

	SceCtrlData pad[64];
	sceCtrlReadBufferPositive(pad, 64);
	checkpointNext("sceCtrlReadBufferPositive:");
	INTR_DISPATCH_TITLE("Bad count", sceCtrlReadBufferPositive(pad, 256));
	INTR_DISPATCH_TITLE("Valid", sceCtrlReadBufferPositive(pad, 64));

	// TODO:
	// sceAudioOutputBlocking / sceAudioOutput / sceAudioSRCOutputBlocking
	// sceGeListSync/sceGeDrawSync
	// sceIoRead/sceIoWrite/sceIoWaitAsync/sceIoGetAsyncStat
	// sceKernelVolatileMemLock
	// sceUmdWaitDriveStat / sceUmdWaitDriveStatWithTimer / sceUmdWaitDriveStatCB

	intrSema = sceKernelCreateSema("sema", 0, 0, 1, NULL);
	intrFlag = sceKernelCreateEventFlag("flag", 0, 0, NULL);
	checkpointNext("Inside interrupt:");
	checkpoint("sceKernelRegisterSubIntrHandler 1: %08x", sceKernelRegisterSubIntrHandler(PSP_VBLANK_INT, 1, (void *)interruptFunc, NULL));
	checkpoint("sceKernelEnableSubIntr: %08x", sceKernelEnableSubIntr(PSP_VBLANK_INT, 1));
	checkpoint("sceKernelDelayThread: %08x", sceKernelDelayThread(30000));
	checkpoint("sceKernelDisableSubIntr: %08x", sceKernelDisableSubIntr(PSP_VBLANK_INT, 1));
	checkpoint("sceKernelReleaseSubIntrHandler: %08x", sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, 1));
	sceKernelDeleteEventFlag(intrFlag);
	sceKernelDeleteSema(intrSema);

	return 0;
}