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
	schedf("[%s] ", ignoreResched == 0 ? (didResched ? "r" : "x") : "?");
	
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

int main(int argc, char *argv[]) {
	reschedThread = sceKernelCreateThread("resched", &reschedFunc, sceKernelGetThreadCurrentPriority(), 0x1000, 0, NULL);

	checkDispatchCases("Semas", &checkSema);
	
	didResched = 0;
	schedf("\n\n");
	checkDispatchCases("Mutexes", &checkMutex);

	return 0;
}