#include <common.h>

#include <pspkernel.h>
#include <pspctrl.h>
#include <psprtc.h>

char schedulingLog[65536];
char *schedulingLogPos;

volatile int didPreempt = 0;
volatile int threadResult = -1;

void outputPadData(int result, SceCtrlData *data) {
	if (data) {
		schedulingLogPos += sprintf(schedulingLogPos, "%08x  %08x (%d, %d)  %02x%02x%02x%02x%02x%02x\n", result, data->Buttons, data->Lx, data->Ly, data->Rsrv[0], data->Rsrv[1], data->Rsrv[2], data->Rsrv[3], data->Rsrv[4], data->Rsrv[5]);
	} else {
		schedulingLogPos += sprintf(schedulingLogPos, "%08x with bad pointer.\n", result);
	}
}

void outputLatchData(int result, SceCtrlLatch *data) {
	if (data) {
		schedulingLogPos += sprintf(schedulingLogPos, "%08x  make=%08x, break=%08x, press=%08x, release=%08x\n", result, data->uiMake, data->uiBreak, data->uiPress, data->uiRelease);
	} else {
		schedulingLogPos += sprintf(schedulingLogPos, "%08x with bad pointer.\n", result);
	}
}

int testThread(SceSize argc, void* argv)
{
	while (1)
	{
		//threadResult = sceUmdWaitDriveStat(0x01);
		//sceKernelRotateThreadReadyQueue(0);
		sceKernelDelayThread(3);
		didPreempt = 1;
		//schedulingLogPos += sprintf(schedulingLogPos, "rescheduled\n");
		//sceKernelRotateThreadReadyQueue(0);
		//threadResult = sceUmdCancelWaitDriveStat();
	}
	return 0;
}

int main(int argc, char *argv[]) {
	int result, data;
	SceCtrlData pad_data[128];
	SceCtrlLatch latch;

	schedulingLogPos = schedulingLog;

	SceUID thread = sceKernelCreateThread("preempt", &testThread, sceKernelGetThreadCurrentPriority() - 1, 0x500, 0, NULL);
	sceKernelStartThread(thread, 0, 0);

	schedulingLogPos += sprintf(schedulingLogPos, "BEFORE sceCtrlSetSamplingMode 1\n");
	didPreempt = 0;
	result = sceCtrlSetSamplingMode(1);
	schedulingLogPos += sprintf(schedulingLogPos, "AFTER sceCtrlSetSamplingMode 1: %08x preempt:%d\n", result, didPreempt);

	sceKernelDelayThread(300);

	schedulingLogPos += sprintf(schedulingLogPos, "BEFORE sceCtrlSetSamplingMode 0\n");
	didPreempt = 0;
	result = sceCtrlSetSamplingMode(0);
	schedulingLogPos += sprintf(schedulingLogPos, "AFTER sceCtrlSetSamplingMode 0: %08x preempt:%d\n", result, didPreempt);

	sceKernelDelayThread(300);

	schedulingLogPos += sprintf(schedulingLogPos, "BEFORE sceCtrlSetSamplingCycle 0\n");
	didPreempt = 0;
	result = sceCtrlSetSamplingCycle(0);
	schedulingLogPos += sprintf(schedulingLogPos, "AFTER sceCtrlSetSamplingCycle 0: %08x preempt:%d\n", result, didPreempt);

	sceKernelDelayThread(300);

	schedulingLogPos += sprintf(schedulingLogPos, "BEFORE sceCtrlGetSamplingMode\n");
	didPreempt = 0;
	result = sceCtrlGetSamplingMode(&data);
	schedulingLogPos += sprintf(schedulingLogPos, "VALUE: %08x\n", data);
	schedulingLogPos += sprintf(schedulingLogPos, "AFTER sceCtrlGetSamplingMode: %08x preempt:%d\n", result, didPreempt);

	sceKernelDelayThread(300);

	schedulingLogPos += sprintf(schedulingLogPos, "BEFORE sceCtrlReadBufferPositive 1\n");
	didPreempt = 0;
	result = sceCtrlReadBufferPositive(&pad_data[0], 1);
	outputPadData(result, &pad_data[0]);
	schedulingLogPos += sprintf(schedulingLogPos, "AFTER sceCtrlReadBufferPositive 1: %08x preempt:%d\n", result, didPreempt);

	sceKernelDelayThread(300);

	schedulingLogPos += sprintf(schedulingLogPos, "BEFORE sceCtrlReadBufferPositive 64\n");
	didPreempt = 0;
	result = sceCtrlReadBufferPositive(&pad_data[0], 64);
	outputPadData(result, &pad_data[0]);
	schedulingLogPos += sprintf(schedulingLogPos, "AFTER sceCtrlReadBufferPositive 64: %08x preempt:%d\n", result, didPreempt);

	sceKernelDelayThread(300);

	schedulingLogPos += sprintf(schedulingLogPos, "BEFORE sceCtrlPeekBufferPositive 64\n");
	didPreempt = 0;
	result = sceCtrlPeekBufferPositive(&pad_data[0], 64);
	outputPadData(result, &pad_data[0]);
	schedulingLogPos += sprintf(schedulingLogPos, "AFTER sceCtrlPeekBufferPositive 64: %08x preempt:%d\n", result, didPreempt);

	sceKernelDelayThread(300);

	schedulingLogPos += sprintf(schedulingLogPos, "BEFORE sceCtrlReadBufferPositive 96\n");
	didPreempt = 0;
	result = sceCtrlReadBufferPositive(&pad_data[0], 96);
	outputPadData(result, &pad_data[0]);
	schedulingLogPos += sprintf(schedulingLogPos, "AFTER sceCtrlReadBufferPositive 96: %08x preempt:%d\n", result, didPreempt);

	sceKernelDelayThread(300);

	schedulingLogPos += sprintf(schedulingLogPos, "BEFORE sceCtrlPeekLatch\n");
	didPreempt = 0;
	result = sceCtrlPeekLatch(&latch);
	// Result is # of reads, which won't match headless.
	result = result >= 1 ? 1 : 0;
	outputLatchData(result, &latch);
	schedulingLogPos += sprintf(schedulingLogPos, "AFTER sceCtrlPeekLatch: %08x preempt:%d\n", result, didPreempt);

	sceKernelDelayThread(300);

	schedulingLogPos += sprintf(schedulingLogPos, "BEFORE sceCtrlReadLatch\n");
	didPreempt = 0;
	result = sceCtrlReadLatch(&latch);
	// Result is # of reads, which won't match headless.
	result = result >= 1 ? 1 : 0;
	outputLatchData(result, &latch);
	schedulingLogPos += sprintf(schedulingLogPos, "AFTER sceCtrlReadLatch: %08x preempt:%d\n", result, didPreempt);

	sceKernelDelayThread(300);

	schedulingLogPos += sprintf(schedulingLogPos, "BEFORE sceCtrlPeekLatch\n");
	didPreempt = 0;
	result = sceCtrlPeekLatch(&latch);
	outputLatchData(result, &latch);
	schedulingLogPos += sprintf(schedulingLogPos, "AFTER sceCtrlPeekLatch: %08x preempt:%d\n", result, didPreempt);

	sceKernelDelayThread(300);

	sceKernelTerminateDeleteThread(thread);
	sceKernelDelayThread(300);
	printf("%s", schedulingLog);

	return 0;
}