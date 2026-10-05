// How sceKernelStartModule and sceKernelStopModule use their SceKernelSMOption: which fields
// override the module_start/module_stop thread's defaults, which values are refused, and which
// stack partitions a user module may ask for.

#include <common.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <pspkernel.h>
#include <pspmodulemgr.h>
#include <string.h>
#include <sysmem-imports.h>

#include "shared.h"

static char childPath[MAXPATHLEN];
static Results results;

static const char *stackRegion(unsigned int addr) {
	addr &= 0x1FFFFFFF;
	if (addr < 0x08400000) {
		return "kernel";
	}
	if (addr < 0x08800000) {
		return "volatile";
	}
	return "user";
}

static void printRecord(const char *label, const ThreadRecord *rec) {
	if (rec->called == 0) {
		printf("    %s thread: didn't run\n", label);
	} else if (rec->called < 0) {
		printf("    %s thread: couldn't refer to itself\n", label);
	} else {
		printf("    %s thread: stack=%08x attr=%08x priority=%02x in %s memory\n", label, rec->stackSize, rec->attr, rec->priority, stackRegion(rec->stack));
	}
}

static SceKernelSMOption *makeOption(SceKernelSMOption *opt, SceSize size, SceUID mpid, SceSize stacksize, int priority, int attr) {
	memset(opt, 0, sizeof(*opt));
	opt->size = size;
	opt->mpidstack = mpid;
	opt->stacksize = stacksize;
	opt->priority = priority;
	opt->attribute = attr;
	return opt;
}

static void printResult(const char *label, int result) {
	if (result >= 0) {
		printf("  %s: OK\n", label);
	} else {
		printf("  %s: %08x\n", label, result);
	}
}

static void testStartStop(const char *title, SceKernelSMOption *startOpt, SceKernelSMOption *stopOpt) {
	memset(&results, 0, sizeof(results));
	Results *ptr = &results;
	int status = 0;

	printf("%s:\n", title);
	SceUID mod = sceKernelLoadModule(childPath, 0, NULL);
	if (mod < 0) {
		printf("  load failed: %08x\n", mod);
		return;
	}

	int result = sceKernelStartModule(mod, sizeof(ptr), &ptr, &status, startOpt);
	printResult("start", result);
	printRecord("start", &results.start);
	if (result >= 0) {
		result = sceKernelStopModule(mod, sizeof(ptr), &ptr, &status, stopOpt);
		printResult("stop", result);
		if (result < 0) {
			result = sceKernelStopModule(mod, sizeof(ptr), &ptr, &status, NULL);
			printResult("stop without option", result);
		}
		printRecord("stop", &results.stop);
	}

	result = sceKernelUnloadModule(mod);
	if (result < 0) {
		printf("  unload failed: %08x\n", result);
	}
}

static void testStart(const char *title, SceKernelSMOption *startOpt) {
	testStartStop(title, startOpt, NULL);
}

static void testStop(const char *title, SceKernelSMOption *stopOpt) {
	testStartStop(title, NULL, stopOpt);
}

int main(int argc, char *argv[]) {
	SceKernelSMOption opt;
	char title[64];
	// Partition 5 (volatile) is left out: on hardware, starting the module with its stack there
	// never returns, and the PSP needs PSPLink restarted afterwards.
	static const int mpids[] = { 0, 1, 2, 3, 4, 6, 7, 8, 9, 12, 13, -1 };

	getcwd(childPath, MAXPATHLEN);
	strcat(childPath, "/child.prx");

	checkpointNext("Defaults:");
	testStart("No option", NULL);
	testStart("Empty option", makeOption(&opt, sizeof(opt), 0, 0, 0, 0));

	checkpointNext("Fields:");
	testStart("Stack 0x2000", makeOption(&opt, sizeof(opt), 0, 0x2000, 0, 0));
	testStart("Stack 0x100", makeOption(&opt, sizeof(opt), 0, 0x100, 0, 0));
	testStart("Priority 0x30", makeOption(&opt, sizeof(opt), 0, 0, 0x30, 0));
	testStart("Priority 0x80", makeOption(&opt, sizeof(opt), 0, 0, 0x80, 0));

	checkpointNext("Attributes:");
	testStart("Attr VFPU", makeOption(&opt, sizeof(opt), 0, 0, 0, PSP_THREAD_ATTR_VFPU));
	testStart("Attr 0x00100000", makeOption(&opt, sizeof(opt), 0, 0, 0, 0x00100000));
	testStart("Attr 0x00002000", makeOption(&opt, sizeof(opt), 0, 0, 0, 0x00002000));
	testStart("Attr user", makeOption(&opt, sizeof(opt), 0, 0, 0, PSP_THREAD_ATTR_USER));
	testStart("Attr kernel", makeOption(&opt, sizeof(opt), 0, 0, 0, 0x00001000));
	testStart("Attr 0x00000001", makeOption(&opt, sizeof(opt), 0, 0, 0, 0x00000001));

	checkpointNext("Stack partitions:");
	for (int i = 0; i < ARRAY_SIZE(mpids); ++i) {
		snprintf(title, sizeof(title), "Partition %d", mpids[i]);
		testStart(title, makeOption(&opt, sizeof(opt), mpids[i], 0, 0, 0));
	}

	checkpointNext("Stop:");
	testStop("Stop stack 0x3000, priority 0x31, VFPU", makeOption(&opt, sizeof(opt), 0, 0x3000, 0x31, PSP_THREAD_ATTR_VFPU));
	testStop("Stop attr 0x00000001", makeOption(&opt, sizeof(opt), 0, 0, 0, 0x00000001));
	testStop("Stop partition 1", makeOption(&opt, sizeof(opt), 1, 0, 0, 0));
	testStop("Stop partition 13", makeOption(&opt, sizeof(opt), 13, 0, 0, 0));

	checkpointNext("Size, before setting an SDK version:");
	testStart("Size 0", makeOption(&opt, 0, 0, 0x2000, 0, 0));
	testStart("Size 0x18", makeOption(&opt, 0x18, 0, 0x2000, 0, 0));

	checkpoint("sceKernelSetCompiledSdkVersion606: %08x", sceKernelSetCompiledSdkVersion606(0x06060010));

	checkpointNext("Size, SDK 6.06:");
	testStart("Size 0", makeOption(&opt, 0, 0, 0x2000, 0, 0));
	testStart("Size 0x10", makeOption(&opt, 0x10, 0, 0x2000, 0, 0));
	testStart("Size 0x14", makeOption(&opt, 0x14, 0, 0x2000, 0, 0));
	testStart("Size 0x18", makeOption(&opt, 0x18, 0, 0x2000, 0, 0));
	testStop("Stop size 0", makeOption(&opt, 0, 0, 0x3000, 0, 0));

	return 0;
}
