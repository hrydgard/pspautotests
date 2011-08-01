#include <common.h>

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <psploadexec.h>

void testSimpleVTimerGet() {
	SceUID vtimer1;

	vtimer1 = sceKernelCreateVTimer("VTIMER1", NULL);
	
	sceKernelDelayThread(10000);
	
	printf("%lld\n", sceKernelGetVTimerTimeWide(vtimer1) / 10000);

	sceKernelStartVTimer(vtimer1);

	sceKernelDelayThread(10000);
	printf("%lld\n", sceKernelGetVTimerTimeWide(vtimer1) / 10000);
	sceKernelStopVTimer(vtimer1);

	sceKernelStartVTimer(vtimer1);

	sceKernelDelayThread(10000);
	printf("%lld\n", sceKernelGetVTimerTimeWide(vtimer1) / 10000);
	sceKernelStopVTimer(vtimer1);

	printf("%lld\n", sceKernelGetVTimerTimeWide(vtimer1) / 10000);
	
	sceKernelStopVTimer(vtimer1);
}

int main(int argc, char **argv) {
	testSimpleVTimerGet();

	return 0;
}