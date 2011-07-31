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

PSP_MODULE_INFO("THREAD TEST", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

static int semaphore = 0;

static int threadFunction(int args, void* argp) {
	int local_value = *(int *)argp;

	eprintf("%d, %d\n", args, local_value);

	sceKernelSignalSema(semaphore, 1);
	
	return 0;
}

void testThreads() {
	int n;

	// Create a semaphore for waiting both threads to execute.
	semaphore = sceKernelCreateSema("Semaphore", 0, 0, 2, NULL);
	
	for (n = 0; n < 2; n++) {
		// Create and start a new thread passing a stack local variable as parameter.
		// When sceKernelStartThread, thread is executed immediately, so in a while it has access
		// to the unmodified stack of the thread that created this one and can access n,
		// before it changes its value.
		sceKernelStartThread(
			sceKernelCreateThread("Test Thread", (void *)&threadFunction, 0x12, 0x10000, 0, NULL),
			n, &n
		);
	}

	// Wait until semaphore have been signaled two times (both new threads have been executed).
	sceKernelWaitSema(semaphore, 2, NULL);

	// After both threads have been executed, we will emit a -1 to check that semaphores work fine.
	eprintf("%d\n", -1);
}

static int threadEndedFunction1(int args, void* argp) {
	eprintf("Thread1.Started\n");
	return 0;
}

static int threadEndedFunction2(int args, void* argp) {
	eprintf("Thread2.Started\n");
	sceKernelExitThread(0);
	return 0;
}

static int threadEndedFunction3(int args, void* argp) {
	eprintf("Thread3.Started\n");
	sceKernelDelayThread(10 * 1000);
	eprintf("Thread3.GoingToEnd\n");
	sceKernelExitThread(0);
	return 0;
}

void testThreadsEnded() {
	int thread1, thread2, thread3, thread4;
	
	// Thread1 will stop returning the function and sceKernelWaitThreadEnd will be executed after the thread have ended.
	thread1 = sceKernelCreateThread("threadEndedFunction1", (void *)&threadEndedFunction1, 0x12, 0x10000, 0, NULL);

	// Thread1 will stop with sceKernelExitThread and sceKernelWaitThreadEnd will be executed after the thread have ended.
	thread2 = sceKernelCreateThread("threadEndedFunction2", (void *)&threadEndedFunction2, 0x12, 0x10000, 0, NULL);

	// Thread3 will stop after a while so it will allow to execute sceKernelWaitThreadEnd before it ends.
	thread3 = sceKernelCreateThread("threadEndedFunction3", (void *)&threadEndedFunction3, 0x12, 0x10000, 0, NULL);
	
	// Thread4 won't start never, so sceKernelWaitThreadEnd can be executed before thread is started.
	thread4 = sceKernelCreateThread("threadEndedFunction4", NULL, 0x12, 0x10000, 0, NULL);

	sceKernelStartThread(thread1, 0, NULL);
	sceKernelStartThread(thread2, 0, NULL);
	sceKernelStartThread(thread3, 0, NULL);
	
	// This waits 5ms and supposes both threads (1 and 2) have ended. Thread 3 should have not ended. Thread 4 is not going to be started.
	sceKernelDelayThread(2 * 1000);

	eprintf("Threads.EndedExpected\n");
	
	sceKernelWaitThreadEnd(thread1, NULL);
	eprintf("Thread1.Ended\n");
	sceKernelWaitThreadEnd(thread2, NULL);
	eprintf("Thread2.Ended\n");
	sceKernelWaitThreadEnd(thread3, NULL);
	eprintf("Thread3.Ended\n");
	sceKernelWaitThreadEnd(thread4, NULL);
	eprintf("Thread4.NotStartedSoEnded\n");
}

int main(int argc, char **argv) {
	pspDebugScreenInit();

	testThreads();
	testThreadsEnded();
	
	return 0;
}