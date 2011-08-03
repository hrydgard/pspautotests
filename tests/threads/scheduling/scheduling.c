#include <common.h>

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <psploadexec.h>

SceUID threads[4];
SceUID sema;

int testSimpleScheduling_Thread(SceSize args, void *argp) {
	unsigned int thread_n = *(unsigned int *)argp;
	int count = 4;
	while (count--) {
		printf("%08X\n", thread_n);
		sceKernelDelayThread(1);
	}
	sceKernelSignalSema(sema, 1);
	
	return 0;
}

void testSimpleScheduling() {
	unsigned int n;
	
	printf("testSimpleScheduling:");
	
	sema = sceKernelCreateSema("EndSema", 0, 0, 4, NULL);
	
	for (n = 0; n < 4; n++) {
		threads[n] = sceKernelCreateThread("Thread-N", testSimpleScheduling_Thread, 0x18, 0x10000, 0, NULL);
		sceKernelStartThread(threads[n], 1, &n);
	}
	
	sceKernelWaitSemaCB(sema, 4, NULL);

	for (n = 0; n < 4; n++) {
		sceKernelDeleteThread(threads[n]);
	}
}

int testSimpleVblankScheduling_Thread(SceSize args, void *argp) {
	unsigned int thread_n = *(unsigned int *)argp;
	int count = 4;
	while (count--) {
		printf("%08X\n", thread_n);
		sceDisplayWaitVblankStart();
	}
	sceKernelSignalSema(sema, 1);
	
	return 0;
}


void testSimpleVblankScheduling() {
	unsigned int n;
	
	printf("testSimpleVblankScheduling:");
	
	sema = sceKernelCreateSema("EndSema", 0, 0, 4, NULL);
	
	for (n = 0; n < 4; n++) {
		threads[n] = sceKernelCreateThread("Thread-N", testSimpleVblankScheduling_Thread, 0x18, 0x10000, 0, NULL);
		sceKernelStartThread(threads[n], 1, &n);
	}
	
	sceKernelWaitSemaCB(sema, 4, NULL);
	
	for (n = 0; n < 4; n++) {
		sceKernelDeleteThread(threads[n]);
	}
}

int main(int argc, char **argv) {
	testSimpleScheduling();
	testSimpleVblankScheduling();

	return 0;
}

