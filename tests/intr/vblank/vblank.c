#include <common.h>

#include <pspkernel.h>
#include <pspthreadman.h>
#include <pspdisplay.h>

SceUID sema = -1;
int vblankCalled = 0;
int mainThreadId = 0;
int called = 0;

int vblankCalledThread = -1;

void vblankCallback(int no, void *value) {
	//sceKernelSignalSema(sema, 1);
	/*
	if (!vblankCalled) {
		vblankCalled = 1;
	}
	*/
	vblankCalledThread = sceKernelGetThreadId();
	called = 1;
	*(uint *)value = 3;
	sceKernelSignalSema(sema, 1);
}

void basicUsage() {
	int value = 7;
	sema = sceKernelCreateSema("semaphore", 0, 0, 255, NULL);
	mainThreadId = sceKernelGetThreadId();

	//int cb = sceKernelCreateCallback("vblankCallback", vblankCallback, NULL);
	sceKernelRegisterSubIntrHandler(PSP_DISPLAY_SUBINT, 0, vblankCallback, &value);
	printf("beforeEnableVblankCallback\n");
	sceKernelEnableSubIntr(PSP_DISPLAY_SUBINT, 0);
	printf("afterEnableVblankCallback\n");
	
	sceKernelWaitSemaCB(sema, 1, NULL);
	//while (!vblankCalled) { sceKernelDelayThread(1000); }
	if (called) {
		printf("vblankCallback(%d):%d\n", *(int *)&value, (vblankCalledThread == mainThreadId));
	}
	
	sceKernelReleaseSubIntrHandler(PSP_DISPLAY_SUBINT, 0);
	//sceDisplayWaitVblank();
	
	printf("ended\n");
}

void vblank_counter(int no, int* counter) {
	*counter = *counter + 1;
}

void suspendUsage() {
	int counter;
	sceKernelRegisterSubIntrHandler(PSP_VBLANK_INT, 0, vblank_counter, &counter);

	counter = 0;
	sceKernelEnableSubIntr(PSP_VBLANK_INT, 0);
	int flag = sceKernelCpuSuspendIntr();
	sceKernelDelayThread(300000);
	sceKernelCpuResumeIntr(flag);
	sceKernelDisableSubIntr(PSP_VBLANK_INT, 0);

	printf("Interrupts suspended: %d\n", counter);

	counter = 0;
	sceKernelRegisterSubIntrHandler(PSP_VBLANK_INT, 0, vblank_counter, &counter);
	sceKernelEnableSubIntr(PSP_VBLANK_INT, 0);
	sceKernelDelayThread(300000);
	sceKernelDisableSubIntr(PSP_VBLANK_INT, 0);

	sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, 0);

	printf("Interrupts resumed: %d\n", counter);
}

int main(int argc, char** argv) {
	basicUsage();
	suspendUsage();
	
	return 0;
}