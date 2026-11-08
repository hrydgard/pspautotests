// The exit status of a thread that returns from its entry function, against one that calls
// sceKernelExitThread, for zero, positive and negative values.

#include <common.h>
#include <pspkernel.h>
#include <pspthreadman.h>

static int returnValue;

static int returnFunc(SceSize argc, void *argp) {
	return returnValue;
}

static int exitFunc(SceSize argc, void *argp) {
	sceKernelExitThread(returnValue);
	return 0x11111111;
}

static void test(const char *title, SceKernelThreadEntry entry, int value) {
	returnValue = value;
	SceUID thread = sceKernelCreateThread(title, entry, 0x10, 0x1000, 0, NULL);
	sceKernelStartThread(thread, 0, NULL);
	int waitResult = sceKernelWaitThreadEnd(thread, NULL);
	int status = sceKernelGetThreadExitStatus(thread);
	printf("%s %08x: wait=%08x, exit status=%08x\n", title, value, waitResult, status);
	sceKernelDeleteThread(thread);
}

int main(int argc, char *argv[]) {
	static const int values[] = { 0, 1, 2, 0x7FFFFFFF, -1, 0x80001234, 0x80000000 };
	int i;
	for (i = 0; i < (int)(sizeof(values) / sizeof(values[0])); ++i) {
		test("return", &returnFunc, values[i]);
	}
	for (i = 0; i < (int)(sizeof(values) / sizeof(values[0])); ++i) {
		test("exit", &exitFunc, values[i]);
	}
	return 0;
}
