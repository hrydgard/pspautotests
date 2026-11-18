#include <common.h>
#include <pspdisplay.h>
#include <pspthreadman.h>
#include <pspintrman.h>

// The vblank waits from an interrupt handler and with dispatch or interrupts disabled, both inside
// and outside a vblank, and sceDisplayGetVblankRest.

extern "C" {
int sceDisplayWaitVblankStartMulti(int vblanks);
int sceDisplayWaitVblankStartMultiCB(int vblanks);
int sceDisplayIsVblank();
int sceDisplayGetVblankRest();
}

extern "C" int HAS_DISPLAY;

static const int FRAME_US = 16683;

static void busyUntil(u32 start, int us) {
	while ((int)(sceKernelGetSystemTimeLow() - start) < us) {
		continue;
	}
}

struct WaitResults {
	int isVblank;
	int rest;
	int wait;
	int waitCB;
	int start;
	int startCB;
	int multi;
	int multiInvalid;
	int vblankAfter;
};

static void runWaits(WaitResults &r) {
	r.isVblank = sceDisplayIsVblank();
	r.rest = sceDisplayGetVblankRest();
	r.wait = sceDisplayWaitVblank();
	r.waitCB = sceDisplayWaitVblankCB();
	r.start = sceDisplayWaitVblankStart();
	r.startCB = sceDisplayWaitVblankStartCB();
	r.multi = sceDisplayWaitVblankStartMulti(2);
	r.multiInvalid = sceDisplayWaitVblankStartMulti(0);
	r.vblankAfter = sceDisplayIsVblank();
}

static const char *restBucket(int rest) {
	if (rest == -1) {
		return "-1";
	}
	if (rest == 0) {
		return "0";
	}
	if (rest >= 1 && rest <= 14) {
		return "1-14";
	}
	return "other";
}

static void printWaits(const char *title, const WaitResults &r) {
	printf("%s:\n", title);
	printf("  IsVblank: %d, GetVblankRest: %s\n", r.isVblank, restBucket(r.rest));
	printf("  WaitVblank: %08x, WaitVblankCB: %08x\n", r.wait, r.waitCB);
	printf("  WaitVblankStart: %08x, WaitVblankStartCB: %08x\n", r.start, r.startCB);
	printf("  Multi(2): %08x, Multi(0): %08x\n", r.multi, r.multiInvalid);
	printf("  IsVblank after: %d\n", r.vblankAfter);
}

static WaitResults alarmResults;
static volatile int alarmDone;

static SceUInt alarmHandler(void *common) {
	runWaits(alarmResults);
	alarmDone = 1;
	return 0;
}

static void testAlarm(const char *title, int delayUs) {
	alarmDone = 0;
	sceDisplayWaitVblankStart();
	SceUID alarm = sceKernelSetAlarm(delayUs, &alarmHandler, NULL);
	if (alarm < 0) {
		printf("%s: sceKernelSetAlarm failed: %08x\n", title, alarm);
		return;
	}
	for (int i = 0; i < 100 && !alarmDone; ++i) {
		sceKernelDelayThread(1000);
	}
	if (!alarmDone) {
		printf("%s: alarm never ran\n", title);
		sceKernelCancelAlarm(alarm);
		return;
	}
	printWaits(title, alarmResults);
}

static void testDispatchDisabled(const char *title, int delayUs) {
	WaitResults r;
	sceDisplayWaitVblankStart();
	u32 start = sceKernelGetSystemTimeLow();
	busyUntil(start, delayUs);
	int state = sceKernelSuspendDispatchThread();
	runWaits(r);
	sceKernelResumeDispatchThread(state);
	printWaits(title, r);
}

static void testIntrDisabled(const char *title, int delayUs) {
	WaitResults r;
	sceDisplayWaitVblankStart();
	u32 start = sceKernelGetSystemTimeLow();
	busyUntil(start, delayUs);
	int state = sceKernelCpuSuspendIntr();
	runWaits(r);
	sceKernelCpuResumeIntr(state);
	printWaits(title, r);
}

static void testVblankRest() {
	printf("sceDisplayGetVblankRest:\n");

	sceDisplayWaitVblankStart();
	u32 start = sceKernelGetSystemTimeLow();
	int early = sceDisplayGetVblankRest();
	busyUntil(start, 600);
	int late = sceDisplayGetVblankRest();
	busyUntil(start, 5000);
	int mid = sceDisplayGetVblankRest();
	printf("  Early in vblank: %s, later: %s, fewer later: %d\n", restBucket(early), restBucket(late), late < early ? 1 : 0);
	printf("  Mid frame: %d\n", mid);

	// Lines left in the vblank, and whether that agrees with sceDisplayIsVblank.
	sceDisplayWaitVblankStart();
	int mismatches = 0;
	int samples = 0;
	int lowest = 100;
	start = sceKernelGetSystemTimeLow();
	while ((int)(sceKernelGetSystemTimeLow() - start) < 1500) {
		int vblank = sceDisplayIsVblank();
		int rest = sceDisplayGetVblankRest();
		int vblank2 = sceDisplayIsVblank();
		if (vblank != vblank2) {
			continue;
		}
		++samples;
		if (vblank ? rest <= 0 : rest > 0) {
			++mismatches;
		}
		if (rest < lowest) {
			lowest = rest;
		}
	}
	printf("  Agrees with IsVblank: %s, lowest: %d\n", samples > 0 && mismatches == 0 ? "yes" : "no", lowest);
}

extern "C" int main(int argc, char *argv[]) {
	HAS_DISPLAY = 0;

	testVblankRest();

	// About 5ms into the frame, and ~300us into the next vblank.
	testAlarm("Alarm mid frame", 5000);
	testAlarm("Alarm in vblank", FRAME_US + 220);

	testDispatchDisabled("Dispatch disabled mid frame", 5000);
	testDispatchDisabled("Dispatch disabled in vblank", 150);

	testIntrDisabled("Interrupts disabled mid frame", 5000);
	testIntrDisabled("Interrupts disabled in vblank", 150);

	return 0;
}
