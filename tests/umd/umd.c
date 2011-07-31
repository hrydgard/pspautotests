#include <pspsdk.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <psploadexec.h>
#include <pspumd.h>

#define eprintf(...) pspDebugScreenPrintf(__VA_ARGS__); Kprintf(__VA_ARGS__);

PSP_MODULE_INFO("UMD TEST", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

int umdHandler(int unknown, int info, void *arg) {
	eprintf("umdHandler called: %08X, %08X, %08X\n", unknown, info, (u32)arg);

	return 0;
}

int main(int argc, char **argv) {
	int umdCbCallbackId;
	int result;

	pspDebugScreenInit();
	
	umdCbCallbackId = sceKernelCreateCallback("umdHandler", umdHandler, (void *)0x1234);
	
	result = sceUmdRegisterUMDCallBack(umdCbCallbackId);
	eprintf("%08X\n", result);
	
	sceKernelCheckCallback();
	
	result = sceUmdUnRegisterUMDCallBack(umdCbCallbackId);
	eprintf("%08X\n", result);

	result = sceUmdUnRegisterUMDCallBack(umdCbCallbackId);
	eprintf("%08X\n", result);

	return 0;
}