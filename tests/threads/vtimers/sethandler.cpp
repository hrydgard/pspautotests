#include "shared.h"

SceUID vtimer;

SceUInt normalHandler(SceUID uid, SceKernelSysClock *scheduled, SceKernelSysClock *actual, void *common) {
	const SceInt64 sched = *(const SceInt64 *)scheduled;
	const SceInt64 act = *(const SceInt64 *)actual;

	if (act < sched) {
		checkpoint("** normalHandler: %08x - called BEFORE schedule, sched=%lld, act=%lld", vtimer == uid ? 0x1337 : uid, sched, act);
		return 1000;
	}

	int late = (act - sched) / 1000;
	checkpoint("** normalHandler: %08x, %lld, +%dms, %08x", vtimer == uid ? 0x1337 : uid, sched, late, common);
	return 1000;
}

const SceInt64 TIME_SEND_NULL = -1337;

int do_sceKernelSetVTimerHandler(SceUID uid, SceInt64 time, int handler, void *common) {
	return sceKernelSetVTimerHandler(uid, time == TIME_SEND_NULL ? NULL : (SceKernelSysClock *)&time, handler ? normalHandler : NULL, common);
}

int do_sceKernelSetVTimerHandlerWide(SceUID uid, SceInt64 time, int handler, void *common) {
	// pspsdk is wrong, they are the same handler.
	return sceKernelSetVTimerHandlerWide(uid, time, handler ? (SceKernelVTimerHandlerWide)normalHandler : NULL, common);
}

typedef int (*VTimerTestFunc)(SceUID uid, SceInt64 time, int handler, void *common);

void runTest(const char *name, int result) {
	checkpoint(NULL);
	schedf(name, result);
	schedf(" - ");
	schedfVTimer(vtimer);
}

void runTests(const char *heading, VTimerTestFunc func) {
	checkpointNext(heading);
	vtimer = sceKernelCreateVTimer("test", NULL);
	SceUID deletedVTimer = sceKernelCreateVTimer("test", NULL);
	sceKernelDeleteVTimer(deletedVTimer);

	checkpointNext("UIDs:");
	runTest("  Normal: %08x", func(vtimer, 0, 1, NULL));
	runTest("  Twice: %08x", func(vtimer, 0, 1, NULL));
	runTest("  NULL: %08x", func(0, 0, 1, NULL));
	runTest("  Invalid: %08x", func(0xDEADBEEF, 0, 1, NULL));
	runTest("  Deleted: %08x", func(deletedVTimer, 0, 1, NULL));
	runTest("  -1: %08x", func(-1, 0, 1, NULL));
	runTest("  Thread ID: %08x", func(sceKernelGetThreadId(), 0, 1, NULL));

	checkpointNext("Times:");
	// Crashes.
	//runTest("  NULL: ", func(vtimer, TIME_SEND_NULL, 1, NULL));
	runTest("  Zero: %08x", func(vtimer, 0, 1, NULL));
	runTest("  Now - 1: %08x", func(vtimer, sceKernelGetSystemTimeWide() - 1, 1, NULL));
	runTest("  Now: %08x", func(vtimer, sceKernelGetSystemTimeWide(), 1, NULL));
	runTest("  Now + 1: %08x", func(vtimer, sceKernelGetSystemTimeWide() + 1, 1, NULL));

	checkpointNext("Handlers:");
	runTest("  Normal: %08x", func(vtimer, 0, 1, NULL));
	runTest("  NULL: %08x", func(vtimer, 0, 0, NULL));

	checkpointNext("Common:");
	runTest("  With handler: %08x", func(vtimer, 0, 1, (void *)0xDEADBEEF));
	runTest("  Without handler: %08x", func(vtimer, 0, 0, (void *)0x12345678));

	/*checkpointNext("While started:");
	sceKernelStartVTimer(vtimer);
	runTest("  With handler: %08x", func(vtimer, 400, 1, (void *)0xDEADC0DE));
	sceKernelDelayThread(500);
	runTest("  With handler: %08x", func(vtimer, 400, 1, (void *)0xDEADC0DE));
	sceKernelDelayThread(500);
	runTest("  Without handler: %08x", func(vtimer, 400, 0, (void *)0xDEADC0DE));
	sceKernelDelayThread(500);
	runTest("  With handler: %08x", func(vtimer, 0, 1, (void *)0xDEADC0DE));
	sceKernelDelayThread(500);
	sceKernelStopVTimer(vtimer);*/

	sceKernelDeleteVTimer(vtimer);
}

extern "C" int main(int argc, char *argv[]) {
	runTests("sceKernelSetVTimerHandler:", &do_sceKernelSetVTimerHandler);
	runTests("sceKernelSetVTimerHandlerWide:", &do_sceKernelSetVTimerHandlerWide);

	// TODO: Both at once?

	return 0;
}