// Starting and stopping a module that has no module_start, module_stop or entry point: what
// sceKernelStartModule and sceKernelStopModule return, and what they leave in the status word.

#include <common.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <pspkernel.h>
#include <pspmodulemgr.h>
#include <string.h>

static char childPath[MAXPATHLEN];

static void printResult(const char *label, SceUID mod, int result) {
	if (result == mod) {
		printf("  %s: module id\n", label);
	} else {
		printf("  %s: %08x\n", label, result);
	}
}

static void testStartStop(const char *title, int withStatus) {
	int status = 0x12345678;
	int *statusPtr = withStatus ? &status : NULL;

	printf("%s\n", title);
	SceUID mod = sceKernelLoadModule(childPath, 0, NULL);
	if (mod < 0) {
		printf("  load failed: %08x\n", mod);
		return;
	}

	int result = sceKernelStartModule(mod, 0, NULL, statusPtr, NULL);
	printResult("start", mod, result);
	if (withStatus) {
		printf("  start status: %08x\n", status);
	}

	status = 0x12345678;
	result = sceKernelStartModule(mod, 0, NULL, statusPtr, NULL);
	printResult("start again", mod, result);
	if (withStatus) {
		printf("  start again status: %08x\n", status);
	}

	status = 0x12345678;
	result = sceKernelStopModule(mod, 0, NULL, statusPtr, NULL);
	printResult("stop", mod, result);
	if (withStatus) {
		printf("  stop status: %08x\n", status);
	}

	result = sceKernelUnloadModule(mod);
	printResult("unload", mod, result);
}

int main(int argc, char *argv[]) {
	getcwd(childPath, MAXPATHLEN);
	strcat(childPath, "/child.prx");

	testStartStop("With a status pointer:", 1);
	testStartStop("Without a status pointer:", 0);

	return 0;
}
