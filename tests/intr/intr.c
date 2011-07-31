//#pragma compile, "%PSPSDK%/bin/psp-gcc" -I. -I"%PSPSDK%/psp/sdk/include" -L. -L"%PSPSDK%/psp/sdk/lib" -D_PSP_FW_VERSION=150 -Wall -g intr.c ../common/emits.c -lpspsdk -lc -lpspuser -lpspkernel -lpsprtc -o intr.elf
//#pragma compile, "%PSPSDK%/bin/psp-fixup-imports" intr.elf

#include <pspsdk.h>
#include <pspkernel.h>
#include <psprtc.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

//#include "../common/emits.h"
#include <pspintrman.h>

PSP_MODULE_INFO("intrtest", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

// http://forums.ps2dev.org/viewtopic.php?t=5687
// @TODO! Fixme! In which thread should handlers be executed?

//#define eprintf(...) pspDebugScreenPrintf(__VA_ARGS__); Kprintf(__VA_ARGS__);

void vblank_handler_counter(int no, int* counter) {
	*counter = *counter + 1;
}

void checkVblankInterruptHandler() {
	int counter = 0, last_counter = 0;
	int results[3], n;

	pspDebugScreenInit();
	pspDebugScreenPrintf("Starting...\n");
	Kprintf("Starting...\n");

	sceKernelRegisterSubIntrHandler(PSP_VBLANK_INT, 0, vblank_handler_counter, &counter);
	sceKernelDelayThread(80000);
	results[0] = counter;
	Kprintf("%d\n", counter); // 0. Not enabled yet.
	
	sceKernelEnableSubIntr(PSP_VBLANK_INT, 0);
	sceKernelDelayThread(160000);
	results[1] = counter;
	Kprintf("%d\n", counter >= 2); // n. Already enabled.

	sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, 0);
	last_counter = counter;
	sceKernelDelayThread(80000);
	results[2] = counter;
	Kprintf("%d\n", last_counter == counter); // n. Disabled.
	
	for (n = 0; n < 3; n++) {
		//Kprintf("Output %d:%d\n", n, results[n]);
		pspDebugScreenPrintf("%d\n", results[n]);
	}
}

/*
// Exit callback
int exitCallback(int arg1, int arg2, void *common) {
	sceKernelExitGame();
	return 0;
}

// Callback thread
int callbackThread(SceSize args, void *argp) {
	int cbid;

	cbid = sceKernelCreateCallback("Exit Callback", (void*) exitCallback, NULL);
	sceKernelRegisterExitCallback(cbid);
	sceKernelSleepThreadCB();

	return 0;
}

// Sets up the callback thread and returns its thread id
int setupCallbacks(void) {
	int thid = 0;

	thid = sceKernelCreateThread("update_thread", callbackThread, 0x11, 0xFA0, 0, 0);
	if (thid >= 0) {
		sceKernelStartThread(thid, 0, 0);
	}
	return thid;
}
*/

int main() {
	//setupCallbacks();
	checkVblankInterruptHandler();

	return 0;
}