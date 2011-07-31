#include <pspsdk.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <psploadexec.h>

#define eprintf(...) pspDebugScreenPrintf(__VA_ARGS__); Kprintf(__VA_ARGS__);

PSP_MODULE_INFO("CHECK UIDS TEST", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

void testFirstUndefinedUids() {
	eprintf("%08X\n", sceKernelWaitEventFlag(1, 1, PSP_EVENT_WAITOR, NULL, NULL));
	eprintf("%08X\n", sceKernelWaitSema(1, 1, NULL));
}

int main(int argc, char **argv) {
	pspDebugScreenInit();
	
	testFirstUndefinedUids();

	return 0;
}