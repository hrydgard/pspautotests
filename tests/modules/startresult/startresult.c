// What sceKernelStartModule and sceKernelStopModule do with the value module_start and
// module_stop return, and which module states each of start, stop and unload accepts.

#include <common.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <pspkernel.h>
#include <pspmodulemgr.h>
#include <string.h>

#include "shared.h"

static char childPath[MAXPATHLEN];
static Control control;
static Control *controlPtr = &control;

static void printResult(const char *label, SceUID mod, int result) {
	if (result == mod) {
		printf("  %s: module id\n", label);
	} else {
		printf("  %s: %08x\n", label, result);
	}
}

static SceUID load(const char *title) {
	printf("%s\n", title);
	memset(&control, 0, sizeof(control));
	SceUID mod = sceKernelLoadModule(childPath, 0, NULL);
	if (mod < 0) {
		printf("  load failed: %08x\n", mod);
	}
	return mod;
}

static void start(SceUID mod, int startReturn) {
	int status = 0x12345678;
	control.startReturn = startReturn;
	int calls = control.startCalls;
	int result = sceKernelStartModule(mod, sizeof(controlPtr), &controlPtr, &status, NULL);
	printResult("start", mod, result);
	printf("    status=%08x, module_start ran %d time(s)\n", status, control.startCalls - calls);
}

static void stop(SceUID mod, int stopReturn) {
	int status = 0x12345678;
	control.stopReturn = stopReturn;
	int calls = control.stopCalls;
	int result = sceKernelStopModule(mod, sizeof(controlPtr), &controlPtr, &status, NULL);
	printResult("stop", mod, result);
	printf("    status=%08x, module_stop ran %d time(s)\n", status, control.stopCalls - calls);
}

static void unload(SceUID mod) {
	printResult("unload", mod, sceKernelUnloadModule(mod));
}

// Quietly gets rid of a module left in any state, so the next case starts clean.
static void cleanup(SceUID mod) {
	control.stopReturn = 0;
	sceKernelStopModule(mod, sizeof(controlPtr), &controlPtr, NULL, NULL);
	sceKernelUnloadModule(mod);
}

static void testStartReturn(const char *title, int startReturn) {
	SceUID mod = load(title);
	if (mod < 0) {
		return;
	}
	start(mod, startReturn);
	stop(mod, 0);
	unload(mod);
	cleanup(mod);
}

static void testStopReturn(const char *title, int stopReturn) {
	SceUID mod = load(title);
	if (mod < 0) {
		return;
	}
	start(mod, 0);
	stop(mod, stopReturn);
	unload(mod);
	stop(mod, 0);
	unload(mod);
	cleanup(mod);
}

int main(int argc, char *argv[]) {
	getcwd(childPath, MAXPATHLEN);
	strcat(childPath, "/child.prx");

	testStartReturn("module_start returns 0:", 0);
	testStartReturn("module_start returns 1:", 1);
	testStartReturn("module_start returns 2:", 2);
	testStartReturn("module_start returns -1:", -1);
	testStartReturn("module_start returns 80001234:", 0x80001234);

	testStopReturn("module_stop returns 1:", 1);
	testStopReturn("module_stop returns 2:", 2);
	testStopReturn("module_stop returns -1:", -1);
	testStopReturn("module_stop returns 80001234:", 0x80001234);

	SceUID mod = load("Stop before start:");
	if (mod >= 0) {
		stop(mod, 0);
		unload(mod);
		cleanup(mod);
	}

	mod = load("Start after stop:");
	if (mod >= 0) {
		start(mod, 0);
		stop(mod, 0);
		start(mod, 0);
		unload(mod);
		cleanup(mod);
	}

	mod = load("Unload while started:");
	if (mod >= 0) {
		start(mod, 0);
		unload(mod);
		stop(mod, 0);
		unload(mod);
		cleanup(mod);
	}

	return 0;
}
