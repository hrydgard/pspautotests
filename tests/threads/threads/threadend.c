#include "shared.h"
#include <psprtc.h>

int fiveFunc(SceSize argc, void *argv) {
	schedf("* fiveFunc\n");
	return 5;
}

int delayFunc(SceSize argc, void *argv) {
	schedf("* delayFunc\n");

	sceKernelDelayThread(500);

	return 7;
}

int neverFunc(SceSize argc, void *argv) {
	sceKernelSleepThread();
	return 8;
}

int suicideFunc(SceSize argc, void *argv) {
	sceKernelExitDeleteThread(9);
	return 9;
}

int deleteFunc(SceSize argc, void *argv) {
	sceKernelDelayThread(100);
	int result = sceKernelTerminateDeleteThread(*(SceUID *) argv);
	schedf("* Delete thread: %08x\n", result);
	return 1;
}

int cbFunc(int arg1, int arg2, void *arg3) {
	schedf("* cbFunc\n");
	return 0;
}

void testWaitEnd(const char *title, SceUID thread, SceUInt timeout) {
	int result = sceKernelWaitThreadEnd(thread, timeout == -1 ? NULL : &timeout);
	schedf("%s: %08x, timeout: %d\n", title, result, timeout);
}

void testWaitEndCB(const char *title, SceUID thread, SceUInt timeout) {
	int result = sceKernelWaitThreadEndCB(thread, timeout == -1 ? NULL : &timeout);
	schedf("%s: %08x, timeout: %d\n", title, result, timeout);
}

int main(int argc, char **argv) {
	SceUID cb = sceKernelCreateCallback("cbFunc", cbFunc, (void *) 0x1234);
	SceUID fiveThread = sceKernelCreateThread("fiveThread", fiveFunc, 0x1F, 0x1000, 0, 0);
	SceUID delayThread = sceKernelCreateThread("delayThread", delayFunc, 0x1F, 0x1000, 0, 0);
	SceUID neverThread = sceKernelCreateThread("neverThread", neverFunc, 0x1F, 0x1000, 0, 0);
	SceUID deleteThread = sceKernelCreateThread("deleteThread", deleteFunc, 0x1F, 0x1000, 0, 0);
	SceUID suicideThread = sceKernelCreateThread("suicideThread", suicideFunc, 0x1F, 0x1000, 0, 0);

	testWaitEnd("Not started", fiveThread, 500);

	sceKernelStartThread(fiveThread, 0, NULL);
	testWaitEnd("Already ended", fiveThread, 500);
	testWaitEnd("Already waited", fiveThread, 500);

	sceKernelStartThread(delayThread, 0, NULL);
	sceKernelTerminateThread(delayThread);
	testWaitEnd("Terminated thread", delayThread, 500);

	SceKernelThreadInfo info;
	info.size = sizeof(info);
	sceKernelReferThreadStatus(neverThread, &info);
	schedf("before start exit=%08x, status=%08x\n", info.exitStatus, info.status);
	sceKernelStartThread(neverThread, 0, NULL);
	sceKernelReferThreadStatus(neverThread, &info);
	schedf("after start exit=%08x, status=%08x\n", info.exitStatus, info.status);

	sceKernelStartThread(delayThread, 0, NULL);
	testWaitEnd("Short timeout", delayThread, 100);

	sceKernelStartThread(neverThread, 0, NULL);
	testWaitEnd("Never wakes", neverThread, 1000);

	sceKernelStartThread(deleteThread, 4, &neverThread);
	sceKernelStartThread(neverThread, 0, NULL);
	testWaitEnd("Thread deleted", neverThread, -1);
	delayThread = sceKernelCreateThread("delayThread", delayFunc, 0x1F, 0x1000, 0, 0);

	sceKernelNotifyCallback(cb, 1);
	sceKernelStartThread(fiveThread, 0, NULL);
	testWaitEnd("Non-CB", fiveThread, 500);
	testWaitEndCB("With CB, already ended", fiveThread, 500);
	
	sceKernelNotifyCallback(cb, 1);
	sceKernelStartThread(delayThread, 0, NULL);
	testWaitEndCB("With CB, short timeout", delayThread, 100);

	testWaitEnd("Invalid", 0xDEADBEEF, 1000);
	testWaitEnd("Zero", 0, 1000);
	testWaitEnd("Self", sceKernelGetThreadId(), 1000);
	testWaitEnd("-1", -1, 1000);
	flushschedf();

	BASIC_SCHED_TEST("Normal",
		result = sceKernelWaitThreadEnd(fiveThread, NULL);
	);
	BASIC_SCHED_TEST("Zero",
		result = sceKernelWaitThreadEnd(0, NULL);
	);

	sceKernelTerminateDeleteThread(delayThread);
	testWaitEnd("Terminated/deleted", delayThread, 1000);
	sceKernelDeleteThread(delayThread);
	testWaitEnd("Deleted", delayThread, 1000);
	sceKernelStartThread(suicideThread, 0, NULL);
	sceKernelDelayThread(500);
	testWaitEnd("Exit deleted", delayThread, 1000);

	flushschedf();
	return 0;
}