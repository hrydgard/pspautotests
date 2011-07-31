/**
 * This feature is used in pspaudio library and probably in a lot of games.
 *
 * It checks also the correct behaviour of semaphores: sceKernelCreateSema, sceKernelSignalSema and sceKernelWaitSema.
 *
 * If new threads are not executed immediately, it would output: 2, 2, -1.
 * It's expected to output 0, 1, -1.
 */
//#pragma compile, "%PSPSDK%/bin/psp-gcc" -I. -I"%PSPSDK%/psp/sdk/include" -L. -L"%PSPSDK%/psp/sdk/lib" -D_PSP_FW_VERSION=150 -Wall -g thread_start.c ../common/emits.c -lpspsdk -lc -lpspuser -lpspkernel -o thread_start.elf
//#pragma compile, "%PSPSDK%/bin/psp-fixup-imports" thread_start.elf

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <psploadexec.h>

#define eprintf(...) pspDebugScreenPrintf(__VA_ARGS__); Kprintf(__VA_ARGS__);
//#define eprintf(...) pspDebugScreenPrintf(__VA_ARGS__);

PSP_MODULE_INFO("T.WAKEUP CONCURRENCY TEST", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

int sema;
int thid;

static int threadFunction1(int args, void* argp) {
	int n;
	for (n = 0; n < 5; n++) {
		eprintf("[1]:%d\n", n);
		sceKernelSignalSema(sema, 1);
		sceKernelSleepThreadCB();
	}
	
	eprintf("[1]\n");
	sceKernelSignalSema(sema, 1);
	
	return 0;
}

void testThreadsWakeup1() {
	int n;
	SceUInt timeout = 20 * 1000; // 20ms
	
	eprintf("------------------------\n");

	sema = sceKernelCreateSema("sema1", 0, 0, 1, NULL);
	sceKernelStartThread(thid = sceKernelCreateThread("Test Thread", (void *)&threadFunction1, 0x12, 0x10000, 0, NULL), 0, NULL);
	
	for (n = 0; n < 5; n++) {
		sceKernelWaitSemaCB(sema, 1, &timeout);
		eprintf("[0]:%d\n", n);
		sceKernelDelayThread(10 * 1000); // 10ms
		sceKernelWakeupThread(thid);
	}
	
	sceKernelWaitSemaCB(sema, 1, &timeout);
	sceKernelTerminateDeleteThread(thid);
	sceKernelDeleteSema(sema);
	
	eprintf("[0]\n");
}

static int threadFunction2(int args, void* argp) {
	sceKernelWaitSemaCB(sema, 1, NULL);

	eprintf("[1]\n");
	sceKernelSleepThreadCB();
	eprintf("[2]\n");
	sceKernelSleepThreadCB();
	eprintf("[3]\n");
	sceKernelSleepThreadCB();
	
	sceKernelDelayThread(10 * 1000);
	
	eprintf("[5]\n");
	
	sceKernelExitThread(777);
	
	return 777;
}

void testThreadsWakeup2() {
	eprintf("------------------------\n");

	sema = sceKernelCreateSema("sema1", 0, 0, 1, NULL);
	thid = sceKernelCreateThread("Test Thread", (void *)&threadFunction2, 0x12, 0x10000, 0, NULL);

	sceKernelStartThread(thid, 0, NULL);

	sceKernelWakeupThread(thid);
	sceKernelWakeupThread(thid);

	eprintf("[0]\n");
	sceKernelSignalSema(sema, 1);
	
	sceKernelDelayThread(10 * 1000);
	
	eprintf("[4]\n");
	
	sceKernelWakeupThread(thid);
	
	sceKernelWaitThreadEndCB(thid, NULL);
	
	eprintf("%d\n", sceKernelGetThreadExitStatus(thid));
	
	sceKernelDeleteThread(thid);
	sceKernelDeleteSema(sema);
}

/*
static int threadFunction3(int args, void* argp) {
	eprintf("[1]\n");
	sceKernelSleepThreadCB();
	eprintf("[2]\n");
	sceKernelSleepThreadCB();
	eprintf("[3]\n");
	sceKernelSleepThreadCB();
	
	sceKernelDelayThread(10 * 1000);
	
	eprintf("[5]\n");
	
	return 777;
}

void testThreadsWakeup3() {
	eprintf("------------------------\n");

	thid = sceKernelCreateThread("Test Thread", (void *)&threadFunction3, 0x12, 0x10000, 0, NULL);

	sceKernelWakeupThread(thid);
	sceKernelWakeupThread(thid);

	sceKernelDelayThread(1 * 1000);
	eprintf("[0]\n");
	
	sceKernelStartThread(thid, 0, NULL);
	
	sceKernelDelayThread(10 * 1000);
	
	eprintf("[4]\n");
	
	sceKernelWakeupThread(thid);
	
	sceKernelWaitThreadEndCB(thid, NULL);
	
	eprintf("%d\n", sceKernelGetThreadExitStatus(thid));
	
	sceKernelDeleteThread(thid);
}
*/

// test3 should check if sceKernelWakeupThread could be called before sceKernelStartThread.

int main(int argc, char **argv) {
	pspDebugScreenInit();

	testThreadsWakeup1();
	testThreadsWakeup2();
	
	return 0;
}