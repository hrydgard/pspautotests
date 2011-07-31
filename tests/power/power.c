#include <pspsdk.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <psploadexec.h>
#include <psppower.h>

#define eprintf(...) pspDebugScreenPrintf(__VA_ARGS__); Kprintf(__VA_ARGS__);

PSP_MODULE_INFO("POWER TEST", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

/**
 * Power Callback Function Definition
 *
 * @param unknown   - unknown function, appears to cycle between 1,2 and 3
 * @param powerInfo - combination of PSP_POWER_CB_ flags
 */
int powerHandler(int unknown, int powerInfo, void *arg) {
	eprintf("powerHandler called: %08X, %08X, %08X\n", unknown, powerInfo, (u32)arg);

	return 0;
}

int main(int argc, char **argv) {
	int powerCbCallbackId;
	int powerCbSlot;
	int result;
	
	pspDebugScreenInit();
	
	powerCbCallbackId = sceKernelCreateCallback("powerHandler", powerHandler, (void *)0x1234);
	eprintf("%d\n", powerCbCallbackId != 0);
	
	powerCbSlot = scePowerRegisterCallback(-1, powerCbCallbackId);
	eprintf("%d\n", powerCbSlot);
	
	sceKernelCheckCallback();
	
	result = scePowerUnregisterCallback(powerCbSlot);
	eprintf("%08X\n", result);

	result = scePowerUnregisterCallback(powerCbSlot);
	eprintf("%08X\n", result);
	
	return 0;
}