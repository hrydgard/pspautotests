#include <pspsdk.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <psploadexec.h>

#define eprintf(...) pspDebugScreenPrintf(__VA_ARGS__); Kprintf(__VA_ARGS__);
//#define eprintf(...) pspDebugScreenPrintf(__VA_ARGS__);

PSP_MODULE_INFO("THREAD EVENTS TEST", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

int result;
SceUID evid;

void testEvents_thread1(int args, void* argp) {
	eprintf("[1]\n");
	sceKernelDelayThread(1000);
	eprintf("[2]\n");
	result = sceKernelSetEventFlag(evid, 4);
	//sceKernelDelayThread(1000);
	//eprintf("[3]\n");
}

void testEvents() {
	SceUInt timeout = 1000;
	u32 outBits = -1;
	evid = sceKernelCreateEventFlag("test_event", PSP_EVENT_WAITMULTIPLE, 4 | 2, NULL);

	outBits = -1;
	result = sceKernelPollEventFlag(evid, 4 | 2, PSP_EVENT_WAITAND, &outBits);
	eprintf("event: %08X:%d\n", result, outBits);

	outBits = -1;
	result = sceKernelPollEventFlag(evid, 8 | 2, PSP_EVENT_WAITAND, &outBits);
	eprintf("event: %08X:%d\n", result, outBits);

	outBits = -1;
	result = sceKernelPollEventFlag(evid, 8 | 4, PSP_EVENT_WAITOR, &outBits);
	eprintf("event: %08X:%d\n", result, outBits);
	result = sceKernelSetEventFlag(evid, 32 | 16);
	eprintf("event: %08X\n", result);
	result = sceKernelClearEventFlag(evid, ~4);
	eprintf("event: %08X\n", result);

	outBits = -1;
	result = sceKernelPollEventFlag(evid, 0xFFFFFFFC, PSP_EVENT_WAITOR, &outBits);
	eprintf("event: %08X:%d\n", result, outBits);

	outBits = -1;
	result = sceKernelPollEventFlag(evid + 100, 8 | 2, PSP_EVENT_WAITAND, &outBits);
	eprintf("event: %08X:%d\n", result, outBits);

	sceKernelStartThread(
		sceKernelCreateThread("Test Thread", (void *)&testEvents_thread1, 0x12, 0x10000, 0, NULL),
		0, NULL
	);

	outBits = -1;
	result = sceKernelWaitEventFlagCB(evid, 4, PSP_EVENT_WAITAND, &outBits, NULL);
	eprintf("event: %08X:%d\n", result, outBits);
	
	result = sceKernelClearEventFlag(evid, ~(2 | 8));

	outBits = -1;
	result = sceKernelWaitEventFlagCB(evid, 2 | 8, PSP_EVENT_WAITAND, &outBits, &timeout);
	eprintf("event: %08X:%d\n", result, outBits);

	result = sceKernelDeleteEventFlag(evid);
	eprintf("event: %08X\n", result);
	
	outBits = -1;
	result = sceKernelPollEventFlag(evid, 8 | 2, PSP_EVENT_WAITAND, &outBits);
	eprintf("event: %08X:%d\n", result, outBits);
	
	// Test PSP_EVENT_WAITCLEARALL
	// Test PSP_EVENT_WAITCLEAR
	// Test callback handling
}

/* Exit callback */
int exitCallback(int arg1, int arg2, void *common) {
	sceKernelExitGame();
	return 0;
}

/* Callback thread */
int callbackThread(SceSize args, void *argp) {
	int cbid;

	cbid = sceKernelCreateCallback("Exit Callback", (void*) exitCallback, NULL);
	sceKernelRegisterExitCallback(cbid);
	sceKernelSleepThreadCB();

	return 0;
}

/* Sets up the callback thread and returns its thread id */
int setupCallbacks(void) {
	int thid = 0;

	thid = sceKernelCreateThread("update_thread", callbackThread, 0x11, 0xFA0, 0, 0);
	if (thid >= 0) {
		sceKernelStartThread(thid, 0, 0);
	}
	return thid;
}

int main(int argc, char **argv) {
	pspDebugScreenInit();
	
	setupCallbacks();

	testEvents();
	
	return 0;
}