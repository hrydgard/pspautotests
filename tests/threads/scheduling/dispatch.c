#define sceKernelCpuResumeIntr sceKernelCpuResumeIntr_WRONG

#include <common.h>

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspthreadman.h>

#undef sceKernelCpuResumeIntr
int sceKernelCpuResumeIntr(int state);

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

typedef struct {
	int count;
	SceUID thread;
	int attr;
	int numWaitThreads;
	SceUID uid;
	int pad[3];
} SceLwMutexWorkarea;

static char schedulingLog[65536];
static volatile int schedulingLogPos = 0;

inline void schedf(const char *format, ...) {
	va_list args;
	va_start(args, format);
	schedulingLogPos += vsprintf(schedulingLog + schedulingLogPos, format, args);
	// This is easier to debug in the emulator, but printf() reschedules on the real PSP.
	//vprintf(format, args);
	va_end(args);
}

inline void flushschedf() {
	printf("%s", schedulingLog);
	schedulingLogPos = 0;
	schedulingLog[0] = '\0';
}

SceUID reschedThread;
volatile int didResched = 0;
int ignoreResched = 0;
int reschedFunc(SceSize argc, void *argp) {
	didResched = 1;
	return 0;
}

void checkpoint(const char *format, ...) {
	int state = sceKernelSuspendDispatchThread();
	sceKernelResumeDispatchThread(state);

	schedf("[%s/%s] ", ignoreResched == 0 ? (didResched ? "r" : "x") : "?", state == 1 ? "y" : "n");

	if (ignoreResched == 0) {
		sceKernelTerminateThread(reschedThread);
	}

	va_list args;
	va_start(args, format);
	schedulingLogPos += vsprintf(schedulingLog + schedulingLogPos, format, args);
	// This is easier to debug in the emulator, but printf() reschedules on the real PSP.
	//vprintf(format, args);
	va_end(args);

	schedf("\n");
	didResched = 0;
	if (ignoreResched == 0) {
		sceKernelStartThread(reschedThread, 0, NULL);
	}
}

SceUID lockThread;

int lockThreadSema(SceSize argc, void *argp) {
	SceUID sema = *(SceUID *)argp;
	checkpoint("T sceKernelWaitSema: %08x", sceKernelWaitSema(sema, 2, NULL));
	checkpoint("T sceKernelDelayThread: %08x", sceKernelDelayThread(3000));
	checkpoint("T sceKernelSignalSema: %08x", sceKernelSignalSema(sema, 1));
	return 0;
}

void startLockThreadSema(SceUID sema) {
	lockThread = sceKernelCreateThread("sema lock", &lockThreadSema, sceKernelGetThreadCurrentPriority() - 1, 0x1000, 0, NULL);
	checkpoint("S sceKernelCreateThread: %08x", lockThread >= 0 ? 1 : lockThread);
	checkpoint("S sceKernelStartThread: %08x", sceKernelStartThread(lockThread, 4, &sema));
}

void endLockThreadSema(SceUID sema) {
	SceUInt timeout = 10000;
	checkpoint("E sceKernelWaitThreadEnd: %08x", sceKernelWaitThreadEnd(lockThread, &timeout));
	checkpoint("E sceKernelTerminateDeleteThread: %08x", sceKernelTerminateDeleteThread(lockThread));
}

void checkSema(int doDispatch) {
	SceUID sema = sceKernelCreateSema("sema", 0, 0, 1, NULL);
	checkpoint("sceKernelCreateSema: %08x", sema >= 0 ? 1 : sema);
	checkpoint("sceKernelSignalSema: %08x", sceKernelSignalSema(sema, 1));
	checkpoint("sceKernelWaitSema: %08x", sceKernelWaitSema(sema, 1, NULL));
	checkpoint("sceKernelWaitSema too much: %08x", sceKernelWaitSema(sema, 9, NULL));
	checkpoint("sceKernelDeleteSema: %08x", sceKernelDeleteSema(sema));
	sema = sceKernelCreateSema("test", 0, 1, 2, NULL);
	checkpoint("sceKernelCreateSema: %08x", sema >= 0 ? 1 : sema);
	startLockThreadSema(sema);
	int state;
	if (doDispatch) {
		++ignoreResched;
		state = sceKernelSuspendDispatchThread();
		checkpoint("sceKernelSuspendDispatchThread: %08x", state);
	}
	SceUInt timeout = 300;
	checkpoint("sceKernelWaitSema: %08x", sceKernelWaitSema(sema, 1, &timeout));
	checkpoint("sceKernelSignalSema: %08x", sceKernelSignalSema(sema, 1));
	if (doDispatch) {
		checkpoint("sceKernelResumeDispatchThread: %08x", sceKernelResumeDispatchThread(state));
		--ignoreResched;
	}
	endLockThreadSema(sema);
	checkpoint("sceKernelPollSema: %08x", sceKernelPollSema(sema, 1));
	checkpoint("sceKernelDeleteSema: %08x", sceKernelDeleteSema(sema));
}

int lockThreadMutex(SceSize argc, void *argp) {
	SceUID mutex = *(SceUID *)argp;
	checkpoint("T sceKernelLockMutex: %08x", sceKernelLockMutex(mutex, 2, NULL));
	checkpoint("T sceKernelDelayThread: %08x", sceKernelDelayThread(3000));
	checkpoint("T sceKernelUnlockMutex: %08x", sceKernelUnlockMutex(mutex, 1));
	return 0;
}

void startLockThreadMutex(SceUID mutex) {
	lockThread = sceKernelCreateThread("mutex lock", &lockThreadMutex, sceKernelGetThreadCurrentPriority() - 1, 0x1000, 0, NULL);
	checkpoint("S sceKernelCreateThread: %08x", lockThread >= 0 ? 1 : lockThread);
	checkpoint("S sceKernelStartThread: %08x", sceKernelStartThread(lockThread, 4, &mutex));
}

void endLockThreadMutex(SceUID mutex) {
	SceUInt timeout = 10000;
	checkpoint("E sceKernelWaitThreadEnd: %08x", sceKernelWaitThreadEnd(lockThread, &timeout));
	checkpoint("E sceKernelTerminateDeleteThread: %08x", sceKernelTerminateDeleteThread(lockThread));
}

void checkMutex(int doDispatch) {
	SceUID mutex = sceKernelCreateMutex("mutex", 0, 1, NULL);
	checkpoint("sceKernelCreateMutex: %08x", mutex >= 0 ? 1 : mutex);
	checkpoint("sceKernelUnlockMutex: %08x", sceKernelUnlockMutex(mutex, 1));
	checkpoint("sceKernelLockMutex: %08x", sceKernelLockMutex(mutex, 1, NULL));
	checkpoint("sceKernelLockMutex invalid: %08x", sceKernelLockMutex(mutex, -1, NULL));
	checkpoint("sceKernelDeleteMutex: %08x", sceKernelDeleteMutex(mutex));
	mutex = sceKernelCreateMutex("test", 0, 1, NULL);
	checkpoint("sceKernelCreateMutex: %08x", mutex >= 0 ? 1 : mutex);
	startLockThreadMutex(mutex);
	int state;
	if (doDispatch) {
		++ignoreResched;
		state = sceKernelSuspendDispatchThread();
		checkpoint("sceKernelSuspendDispatchThread: %08x", state);
	}
	SceUInt timeout = 300;
	checkpoint("sceKernelLockMutex: %08x", sceKernelLockMutex(mutex, 1, &timeout));
	checkpoint("sceKernelUnlockMutex: %08x", sceKernelUnlockMutex(mutex, 1));
	if (doDispatch) {
		checkpoint("sceKernelResumeDispatchThread: %08x", sceKernelResumeDispatchThread(state));
		--ignoreResched;
	}
	endLockThreadMutex(mutex);
	checkpoint("sceKernelTryLockMutex: %08x", sceKernelTryLockMutex(mutex, 1));
	checkpoint("sceKernelDeleteMutex: %08x", sceKernelDeleteMutex(mutex));
}

int lockThreadLwMutex(SceSize argc, void *argp) {
	void *mutex = *(void **)argp;
	checkpoint("T sceKernelLockLwMutex: %08x", sceKernelLockLwMutex(mutex, 2, NULL));
	checkpoint("T sceKernelDelayThread: %08x", sceKernelDelayThread(3000));
	checkpoint("T sceKernelUnlockLwMutex: %08x", sceKernelUnlockLwMutex(mutex, 1));
	return 0;
}

void startLockThreadLwMutex(void *mutex) {
	lockThread = sceKernelCreateThread("lwmutex lock", &lockThreadLwMutex, sceKernelGetThreadCurrentPriority() - 1, 0x1000, 0, NULL);
	checkpoint("S sceKernelCreateThread: %08x", lockThread >= 0 ? 1 : lockThread);
	checkpoint("S sceKernelStartThread: %08x", sceKernelStartThread(lockThread, 4, &mutex));
}

void endLockThreadLwMutex(void *mutex) {
	SceUInt timeout = 10000;
	checkpoint("E sceKernelWaitThreadEnd: %08x", sceKernelWaitThreadEnd(lockThread, &timeout));
	checkpoint("E sceKernelTerminateDeleteThread: %08x", sceKernelTerminateDeleteThread(lockThread));
}

void checkLwMutex(int doDispatch) {
	SceLwMutexWorkarea workarea;
	checkpoint("sceKernelCreateLwMutex: %08x", sceKernelCreateLwMutex(&workarea, "lwmutex", 0, 1, NULL));
	checkpoint("sceKernelUnlockLwMutex: %08x", sceKernelUnlockLwMutex(&workarea, 1));
	checkpoint("sceKernelLockLwMutex: %08x", sceKernelLockLwMutex(&workarea, 1, NULL));
	checkpoint("sceKernelLockLwMutex invalid: %08x", sceKernelLockLwMutex(&workarea, -1, NULL));
	checkpoint("sceKernelDeleteLwMutex: %08x", sceKernelDeleteLwMutex(&workarea));
	checkpoint("sceKernelCreateLwMutex: %08x", sceKernelCreateLwMutex(&workarea, "lwmutex", 0, 1, NULL));
	startLockThreadLwMutex(&workarea);
	int state;
	if (doDispatch) {
		++ignoreResched;
		state = sceKernelSuspendDispatchThread();
		checkpoint("sceKernelSuspendDispatchThread: %08x", state);
	}
	SceUInt timeout = 300;
	checkpoint("sceKernelLockLwMutex: %08x", sceKernelLockLwMutex(&workarea, 1, &timeout));
	checkpoint("sceKernelUnlockLwMutex: %08x", sceKernelUnlockLwMutex(&workarea, 1));
	if (doDispatch) {
		checkpoint("sceKernelResumeDispatchThread: %08x", sceKernelResumeDispatchThread(state));
		--ignoreResched;
	}
	endLockThreadLwMutex(&workarea);
	checkpoint("sceKernelTryLockLwMutex: %08x", sceKernelTryLockLwMutex_600(&workarea, 1));
	checkpoint("sceKernelDeleteLwMutex: %08x", sceKernelDeleteLwMutex(&workarea));
}

int lockThreadEventFlag(SceSize argc, void *argp) {
	SceUID flag = *(SceUID *)argp;
	checkpoint("T sceKernelWaitEventFlag: %08x", sceKernelWaitEventFlag(flag, 3, PSP_EVENT_WAITAND, NULL, NULL));
	checkpoint("T sceKernelDelayThread: %08x", sceKernelDelayThread(3000));
	checkpoint("T sceKernelClearEventFlag: %08x", sceKernelClearEventFlag(flag, 1));
	return 0;
}

void startLockThreadEventFlag(SceUID flag) {
	lockThread = sceKernelCreateThread("eventflag lock", &lockThreadEventFlag, sceKernelGetThreadCurrentPriority() - 1, 0x1000, 0, NULL);
	checkpoint("S sceKernelCreateThread: %08x", lockThread >= 0 ? 1 : lockThread);
	checkpoint("S sceKernelStartThread: %08x", sceKernelStartThread(lockThread, 4, &flag));
}

void endLockThreadEventFlag(SceUID flag) {
	SceUInt timeout = 10000;
	checkpoint("E sceKernelWaitThreadEnd: %08x", sceKernelWaitThreadEnd(lockThread, &timeout));
	checkpoint("E sceKernelTerminateDeleteThread: %08x", sceKernelTerminateDeleteThread(lockThread));
}

void checkEventFlag(int doDispatch) {
	SceUID flag = sceKernelCreateEventFlag("eventflag", 0, 0xFFFFFFFF, NULL);
	checkpoint("sceKernelCreateEventFlag: %08x", flag >= 0 ? 1 : flag);
	checkpoint("sceKernelClearEventFlag: %08x", sceKernelClearEventFlag(flag, 1));
	checkpoint("sceKernelWaitEventFlag: %08x", sceKernelWaitEventFlag(flag, 1, PSP_EVENT_WAITAND, NULL, NULL));
	checkpoint("sceKernelWaitEventFlag invalid: %08x", sceKernelWaitEventFlag(flag, 0, 0, NULL, NULL));
	checkpoint("sceKernelDeleteEventFlag: %08x", sceKernelDeleteEventFlag(flag));
	flag = sceKernelCreateEventFlag("test", 0, 0xFFFFFFFF, NULL);
	checkpoint("sceKernelCreateEventFlag: %08x", flag >= 0 ? 1 : flag);
	startLockThreadEventFlag(flag);
	int state;
	if (doDispatch) {
		++ignoreResched;
		state = sceKernelSuspendDispatchThread();
		checkpoint("sceKernelSuspendDispatchThread: %08x", state);
	}
	SceUInt timeout = 300;
	checkpoint("sceKernelWaitEventFlag: %08x", sceKernelWaitEventFlag(flag, 1, PSP_EVENT_WAITAND, NULL, &timeout));
	checkpoint("sceKernelClearEventFlag: %08x", sceKernelClearEventFlag(flag, 1));
	if (doDispatch) {
		checkpoint("sceKernelResumeDispatchThread: %08x", sceKernelResumeDispatchThread(state));
		--ignoreResched;
	}
	endLockThreadEventFlag(flag);
	checkpoint("sceKernelPollEventFlag: %08x", sceKernelPollEventFlag(flag, 1, PSP_EVENT_WAITAND, NULL));
	checkpoint("sceKernelDeleteEventFlag: %08x", sceKernelDeleteEventFlag(flag));
}

void checkIo(int doDispatch) {
	char temp[128];
	SceUID fd = sceIoOpen("dispatch.prx", PSP_O_RDONLY, 0777);
	checkpoint("sceIoOpen: %08x", fd >= 0 ? 1 : fd);
	checkpoint("sceIoRead: %08x", sceIoRead(fd, temp, sizeof(temp)));
	checkpoint("sceIoClose: %08x", sceIoClose(fd));

	int state;
	if (doDispatch) {
		++ignoreResched;
		state = sceKernelSuspendDispatchThread();
		checkpoint("sceKernelSuspendDispatchThread: %08x", state);
	}
	fd = sceIoOpen("dispatch.prx", PSP_O_RDONLY, 0777);
	checkpoint("sceIoOpen: %08x", fd >= 0 ? 1 : fd);
	checkpoint("sceIoRead: %08x", sceIoRead(fd, temp, sizeof(temp)));
	checkpoint("sceIoClose: %08x", sceIoClose(fd));
	if (doDispatch) {
		checkpoint("sceKernelResumeDispatchThread: %08x", sceKernelResumeDispatchThread(state));
		--ignoreResched;
	}

	SceInt64 res = -1;
	int result = -1;
	fd = sceIoOpenAsync("dispatch.prx", PSP_O_RDONLY, 0777);
	checkpoint("sceIoOpenAsync: %08x", fd >= 0 ? 1 : fd);
	if (doDispatch) {
		++ignoreResched;
		state = sceKernelSuspendDispatchThread();
		checkpoint("sceKernelSuspendDispatchThread: %08x", state);
	}
	result = sceIoPollAsync(fd, &res);
	checkpoint("sceIoPollAsync: %08x / %016llx", result, res >= 0 ? 1LL : res);
	result = sceIoGetAsyncStat(fd, 1, &res);
	checkpoint("sceIoGetAsyncStat: %08x / %016llx", result, res >= 0 ? 1LL : res);
	result = sceIoGetAsyncStat(fd, 0, &res);
	checkpoint("sceIoGetAsyncStat: %08x / %016llx", result, res >= 0 ? 1LL : res);
	result = sceIoWaitAsync(fd, &res);
	checkpoint("sceIoWaitAsync: %08x / %016llx", result, res >= 0 ? 1LL : res);
	if (doDispatch) {
		checkpoint("sceKernelResumeDispatchThread: %08x", sceKernelResumeDispatchThread(state));
		--ignoreResched;
	}
	result = sceIoWaitAsync(fd, &res);
	checkpoint("sceIoWaitAsync: %08x / %016llx", result, res >= 0 ? 1LL : res);
	if (doDispatch) {
		++ignoreResched;
		state = sceKernelSuspendDispatchThread();
		checkpoint("sceKernelSuspendDispatchThread: %08x", state);
	}
	checkpoint("sceIoRead: %08x", sceIoRead(fd, temp, sizeof(temp)));
	checkpoint("sceIoWrite: %08x", sceIoWrite(1, "Hello.", sizeof("Hello.")));
	if (doDispatch) {
		checkpoint("sceKernelResumeDispatchThread: %08x", sceKernelResumeDispatchThread(state));
		--ignoreResched;
	}
	checkpoint("sceIoCloseAsync: %08x", sceIoCloseAsync(fd));
	result = sceIoWaitAsync(fd, &res);
	checkpoint("sceIoWaitAsync: %08x / %016llx", result, res);
}

void checkDispatchCases(const char *name, void (*testfunc)(int)) {
	int state;

	checkpoint("%s without changes:", name);
	testfunc(0);
	flushschedf();
	
	didResched = 0;
	schedf("\n");
	checkpoint("%s with short dispatch suspend:", name);
	testfunc(1);
	flushschedf();

	didResched = 0;
	schedf("\n");
	checkpoint("%s while dispatch suspended:", name);
	// Starting a thread apparently resumes the dispatch thread.
	++ignoreResched;
	state = sceKernelSuspendDispatchThread();
	checkpoint("sceKernelSuspendDispatchThread: %08x", state);
	testfunc(0);
	checkpoint("sceKernelResumeDispatchThread: %08x", sceKernelResumeDispatchThread(state));
	--ignoreResched;
	flushschedf();

	didResched = 0;
	schedf("\n");
	checkpoint("%s while intr suspended:", name);
	state = sceKernelCpuSuspendIntr();
	checkpoint("sceKernelCpuSuspendIntr: %08x", state);
	testfunc(1);
	checkpoint("sceKernelCpuResumeIntr: %08x", sceKernelCpuResumeIntr(state));
	flushschedf();
}

void vblankCallback(int no, void *value) {
	checkpoint("vblankCallback");
}

void checkDispatchInterrupt() {
	checkpoint("Interrupts while dispatch disabled:");

	sceKernelRegisterSubIntrHandler(PSP_VBLANK_INT, 0, &vblankCallback, NULL);
	sceKernelEnableSubIntr(PSP_VBLANK_INT, 0);

	++ignoreResched;
	int state = sceKernelSuspendDispatchThread();

	int base = sceDisplayGetVcount();
	int i, j;
	for (i = 0; i < 1000; ++i) {
		if (sceDisplayGetVcount() > base + 3) {
			break;
		}
		for (j = 0; j < 10000; ++j)
			continue;
	}

	checkpoint("vblanks=%d", sceDisplayGetVcount() - base);

	sceKernelResumeDispatchThread(state);
	--ignoreResched;

	base = sceDisplayGetVcount();
	for (i = 0; i < 1000; ++i) {
		if (sceDisplayGetVcount() > base + 3) {
			break;
		}
		for (j = 0; j < 10000; ++j)
			continue;
	}
	
	checkpoint("vblanks=%d", sceDisplayGetVcount() - base);

	sceKernelDisableSubIntr(PSP_VBLANK_INT, 0);
	sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, 0);
	flushschedf();
}

int main(int argc, char *argv[]) {
	reschedThread = sceKernelCreateThread("resched", &reschedFunc, sceKernelGetThreadCurrentPriority(), 0x1000, 0, NULL);

	checkDispatchCases("Semas", &checkSema);
	
	didResched = 0;
	schedf("\n\n");
	checkDispatchCases("Mutexes", &checkMutex);
	
	didResched = 0;
	schedf("\n\n");
	checkDispatchCases("LwMutexes", &checkLwMutex);
	
	didResched = 0;
	schedf("\n\n");
	checkDispatchCases("EventFlags", &checkEventFlag);
	
	didResched = 0;
	schedf("\n\n");
	checkDispatchCases("Io", &checkIo);
	
	didResched = 0;
	schedf("\n\n");
	checkDispatchInterrupt();

	return 0;
}