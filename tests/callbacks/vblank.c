#include <common.h>

#include <pspkernel.h>
#include <pspthreadman.h>

int pointer;

SceUID sema;
int vblankCalled = 0;

void vblankCallback(int no, void *value) {
	//sceKernelSignalSema(sema, 1);
	/*
	if (!vblankCalled) {
		vblankCalled = 1;
	}
	*/
	sceKernelSignalSema(sema, 1);
	printf("vblankCallback(%d)\n", *(int *)value);
}

int main(int argc, char** argv) {
	int value = 7;
	sema = sceKernelCreateSema("semaphore", 0, 0, 255, NULL);

	//int cb = sceKernelCreateCallback("vblankCallback", vblankCallback, NULL);
	sceKernelRegisterSubIntrHandler(PSP_DISPLAY_SUBINT, 0, vblankCallback, &value);
	printf("beforeEnableVblankCallback\n");
	sceKernelEnableSubIntr(PSP_DISPLAY_SUBINT, 0);
	printf("afterEnableVblankCallback\n");
	
	sceKernelWaitSemaCB(sema, 1, NULL);
	//while (!vblankCalled) { sceKernelDelayThread(1000); }
	
	printf("ended\n");
	
	sceKernelExitGame();
	return 0;
}