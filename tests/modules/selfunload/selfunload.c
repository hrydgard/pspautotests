// The ModuleMgr functions that act on "the caller's module": sceKernelGetModuleId, the three
// self stop/unload calls, and the address lookups. The child module makes the calls from its own
// code, on a thread this module created, so the caller's code and the thread's creator differ.

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
static SceUID mainId;

static const char *describeId(SceUID mod, int result) {
	static char buf[32];
	if (result == mod) {
		return "child";
	}
	if (result == mainId) {
		return "main";
	}
	snprintf(buf, sizeof(buf), "%08x", result);
	return buf;
}

static SceUID loadAndStart() {
	memset(&control, 0, sizeof(control));
	SceUID mod = sceKernelLoadModule(childPath, 0, NULL);
	if (mod < 0) {
		printf("  load failed: %08x\n", mod);
		return mod;
	}
	int result = sceKernelStartModule(mod, sizeof(controlPtr), &controlPtr, NULL, NULL);
	if (result < 0) {
		printf("  start failed: %08x\n", result);
		sceKernelUnloadModule(mod);
		return result;
	}
	return mod;
}

static void cleanup(SceUID mod) {
	control.stopReturn = 0;
	sceKernelStopModule(mod, 0, NULL, NULL, NULL);
	sceKernelUnloadModule(mod);
}

static void testSelfUnload(const char *title, int call, int exitCode, int stopReturn) {
	printf("%s\n", title);
	SceUID mod = loadAndStart();
	if (mod < 0) {
		return;
	}

	control.call = call;
	control.exitCode = exitCode;
	control.stopReturn = stopReturn;
	control.status = 0x12345678;

	SceUID thread = sceKernelCreateThread("worker", control.worker, 0x18, 0x1000, 0, NULL);
	sceKernelStartThread(thread, 0, NULL);
	int waitResult = sceKernelWaitThreadEnd(thread, NULL);
	sceKernelDeleteThread(thread);

	printf("  wait: %08x\n", waitResult);
	if (control.callReturned) {
		printf("  call returned %08x\n", control.callResult);
	} else {
		printf("  call didn't return\n");
	}
	printf("  module_stop ran %d time(s)", control.stopCalls);
	if (control.stopCalls) {
		printf(", argSize=%d, arg=%08x", control.stopArgSize, control.stopArg);
	}
	printf("\n  status=%08x\n", control.status);

	SceKernelModuleInfo info;
	info.size = sizeof(info);
	int query = sceKernelQueryModuleInfo(mod, &info);
	printf("  module %s\n", query == 0 ? "still loaded" : "gone");
	if (query == 0) {
		cleanup(mod);
	}
}

// Where a module's address range ends: the text, data and bss sizes it reports, from its text.
static void testRangeEnd(const char *name, SceUID query, SceUID child) {
	SceKernelModuleInfo info;
	info.size = sizeof(info);
	if (sceKernelQueryModuleInfo(query, &info) < 0) {
		printf("  %s: query failed\n", name);
		return;
	}
	u32 end = info.text_addr + info.text_size + info.data_size + info.bss_size;
	printf("  GetModuleIdByAddress(%s text + sizes - 4): %s\n", name, describeId(child, control.getModuleIdByAddress(end - 4)));
	printf("  GetModuleIdByAddress(%s text + sizes): %s\n", name, describeId(child, control.getModuleIdByAddress(end)));
}

static void testLookups() {
	printf("Lookups from the child's code:\n");
	SceUID mod = loadAndStart();
	if (mod < 0) {
		return;
	}

	printf("  GetModuleId: %s\n", describeId(mod, control.getModuleId()));
	printf("  GetModuleIdByAddress(child text): %s\n", describeId(mod, control.getModuleIdByAddress((u32)control.worker)));
	// The child's data sits after an alignment gap the sizes don't cover, so it's past the range.
	printf("  GetModuleIdByAddress(child data): %s\n", describeId(mod, control.getModuleIdByAddress(control.childData)));
	printf("  GetModuleIdByAddress(main data): %s\n", describeId(mod, control.getModuleIdByAddress((u32)&control)));
	testRangeEnd("child", mod, mod);
	testRangeEnd("main", mainId, mod);
	printf("  GetModuleIdByAddress(0): %s\n", describeId(mod, control.getModuleIdByAddress(0)));
	printf("  GetModuleIdByAddress(88000000): %s\n", describeId(mod, control.getModuleIdByAddress(0x88000000)));

	u32 gp = 0x12345678;
	int result = control.getModuleGPByAddress((u32)control.worker, &gp);
	printf("  GetModuleGPByAddress(child text): %08x, gp %s\n", result, gp == 0x12345678 ? "untouched" : "written");
	gp = 0x12345678;
	result = control.getModuleGPByAddress(0, &gp);
	printf("  GetModuleGPByAddress(0): %08x, gp %s\n", result, gp == 0x12345678 ? "untouched" : "written");
	gp = 0x12345678;
	result = control.getModuleGPByAddress(0x88000000, &gp);
	printf("  GetModuleGPByAddress(88000000): %08x, gp %s\n", result, gp == 0x12345678 ? "untouched" : "written");
	result = control.getModuleGPByAddress((u32)control.worker, NULL);
	printf("  GetModuleGPByAddress(child text, NULL): %08x\n", result);
	result = control.getModuleGPByAddress((u32)control.worker, (u32 *)0x88000000);
	printf("  GetModuleGPByAddress(child text, 88000000): %08x\n", result);

	cleanup(mod);
}

int main(int argc, char *argv[]) {
	getcwd(childPath, MAXPATHLEN);
	strcat(childPath, "/child.prx");
	mainId = sceKernelGetModuleIdByAddress(&main);

	testLookups();

	testSelfUnload("SelfStopUnloadModule, module_stop returns 0:", CALL_SELF_STOP_UNLOAD, 0x1234, 0);
	testSelfUnload("SelfStopUnloadModule, module_stop returns 1:", CALL_SELF_STOP_UNLOAD, 0x1234, 1);
	testSelfUnload("SelfStopUnloadModule, module_stop returns 2:", CALL_SELF_STOP_UNLOAD, 0x1234, 2);
	testSelfUnload("SelfStopUnloadModule, exit code -1:", CALL_SELF_STOP_UNLOAD, -1, 0);
	testSelfUnload("StopUnloadSelfModule, module_stop returns 0:", CALL_STOP_UNLOAD_SELF, 0, 0);
	testSelfUnload("StopUnloadSelfModule, module_stop returns 2:", CALL_STOP_UNLOAD_SELF, 0, 2);
	testSelfUnload("StopUnloadSelfModuleWithStatus, module_stop returns 0:", CALL_STOP_UNLOAD_SELF_WITH_STATUS, 0x1234, 0);
	testSelfUnload("StopUnloadSelfModuleWithStatus, module_stop returns 2:", CALL_STOP_UNLOAD_SELF_WITH_STATUS, 0x1234, 2);
	testSelfUnload("StopUnloadSelfModule, kernel status pointer:", CALL_STOP_UNLOAD_SELF_BAD_STATUS, 0, 0);

	return 0;
}
