#include <common.h>

#include <pspkernel.h>
#include <pspsysmem.h>
#include <pspthreadman.h>

// printf() seems to reschedule, so can't use it.
static char schedulingLog[65536];
static volatile int schedulingLogPos = 0;

inline void schedf(const char *format, ...) {
	va_list args;
	va_start(args, format);
	schedulingLogPos += vsprintf(schedulingLog + schedulingLogPos, format, args);
	// This is easier to debug in the emulator, but printf() reschedules on the real PSP.
	//vprintf(format, args);
	va_end(args);
}

inline void flushschedf() {
	printf("%s", schedulingLog);
	schedulingLogPos = 0;
	schedulingLog[0] = '\0';
}

int testThreadStackFillsWith_0xFF_AndIsAlignedTo_0x10__thread(SceSize arglen, void *argp) {
	sceKernelSleepThread();

	return 0;
}

void testThreadStackFillsWith_0xFF_AndIsAlignedTo_0x10() {
	int n, FFLen = 0;
	SceKernelThreadInfo info;

	printf("testThreadStackFillsWith_0xFF_AndIsAlignedTo_0x10:\n");
	SceUID thid = sceKernelCreateThread("my_thread", &testThreadStackFillsWith_0xFF_AndIsAlignedTo_0x10__thread, 0x18, 0x201, 0, NULL);
	sceKernelStartThread(thid, 0, NULL);

	info.size = sizeof(info);
	sceKernelReferThreadStatus(thid, &info);
	schedf("StackStart: %08X\n", (unsigned int)((char *)info.stack - (char *)&info));
	printf("StackSize   : %08X\n", info.stackSize);
	for (n = 0; n < info.stackSize; n++) {
		if (((unsigned char *)info.stack)[-n] == 0xFF) FFLen++;
	}
	printf("FFLen       : %08X\n", FFLen);
	printf("Aligned_0x10: %s\n"  , (((unsigned int)info.stack) & 0xF) == 0 ? "yes" : "no");

	sceKernelWakeupThread(thid);
	sceKernelWaitThreadEnd(thid, NULL);
	flushschedf();
}

int main(int argc, char **argv) {
	testThreadStackFillsWith_0xFF_AndIsAlignedTo_0x10();
	
	return 0;
}
