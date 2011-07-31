#include <pspkernel.h>
#include <pspctrl.h>
#include <psprtc.h>

#define eprintf(...) pspDebugScreenPrintf(__VA_ARGS__); Kprintf(__VA_ARGS__);

PSP_MODULE_INFO("ctrl test", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

void testControllerTimmings() {
	u64 tick0, tick1;
	int n;
	SceCtrlData pad_data;
	SceCtrlLatch latch;
	
	// Test sceCtrlReadBufferPositive
	sceRtcGetCurrentTick(&tick0);
	for (n = 0; n < 5; n++) sceCtrlReadBufferPositive(&pad_data, 1);
	sceRtcGetCurrentTick(&tick1);
	eprintf("%d\n", (tick1 - tick0 > 5000));

	// Test sceCtrlReadLatch
	sceRtcGetCurrentTick(&tick0);
	for (n = 0; n < 5; n++) sceCtrlReadLatch(&latch);
	sceRtcGetCurrentTick(&tick1);
	eprintf("%d\n", (tick1 - tick0 > 5000));

	// Test sceCtrlPeekBufferPositive
	sceRtcGetCurrentTick(&tick0);
	for (n = 0; n < 5; n++) sceCtrlPeekBufferPositive(&pad_data, 1);
	sceRtcGetCurrentTick(&tick1);
	eprintf("%d\n", (tick1 - tick0 < 5000));

	// Test sceCtrlPeekLatch
	/*
	sceRtcGetCurrentTick(&tick0);
	for (n = 0; n < 5; n++) sceCtrlPeekLatch(&pad_data, 1);
	sceRtcGetCurrentTick(&tick1);
	eprintf("%d\n", (tick1 - tick0 < 5000));
	*/
}

int main(int argc, char *argv[]) {
	pspDebugScreenInit();
	
	testControllerTimmings();

	return 0;
}