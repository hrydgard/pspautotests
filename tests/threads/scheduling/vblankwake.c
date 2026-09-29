#include <common.h>
#include <pspthreadman.h>
#include <pspdisplay.h>
#include <pspintrman.h>

// Several threads wait in sceDisplayWaitVblankStart: in what order do they wake, and how long after
// the vblank interrupt (timed by a vblank handler) does the first get the CPU? Main waits too, at
// 0x20. Each one after that follows 20-35us later, too close to call in buckets.

static volatile u32 intrAt;
static u32 wokeAt[8];
static char order[16];
static int orderLen = 0;

static void vblankHandler(int no, void *arg) {
	intrAt = sceKernelGetSystemTimeLow();
}

static int waiter(SceSize argc, void *argp) {
	int index = *(int *)argp;
	sceDisplayWaitVblankStart();
	wokeAt[index] = sceKernelGetSystemTimeLow();
	order[orderLen++] = 'A' + index;
	return 0;
}

static const char *bucket(u32 us) {
	if (us < 25) {
		return "<25us";
	} else if (us < 50) {
		return "25-50us";
	} else if (us < 100) {
		return "50-100us";
	}
	return "later";
}

static void run(const char *title, const int *prios, int count) {
	SceUID threads[8];
	// Start them just after a vblank, so they all wait for the same next one.
	sceDisplayWaitVblankStart();
	orderLen = 0;
	for (int i = 0; i < count; ++i) {
		threads[i] = sceKernelCreateThread("waiter", &waiter, prios[i], 0x1000, 0, NULL);
		sceKernelStartThread(threads[i], sizeof(i), &i);
	}
	sceDisplayWaitVblankStart();
	u32 mainAt = sceKernelGetSystemTimeLow();
	order[orderLen++] = 'M';
	sceKernelDelayThread(2000);
	order[orderLen] = 0;
	schedf("  %s: %s\n", title, order);
	u32 first = mainAt - intrAt;
	for (int i = 0; i < count; ++i) {
		first = wokeAt[i] - intrAt < first ? wokeAt[i] - intrAt : first;
		sceKernelWaitThreadEnd(threads[i], NULL);
		sceKernelDeleteThread(threads[i]);
	}
	schedf("    the first ran %s after the interrupt\n", bucket(first));
}

int main(int argc, char *argv[]) {
	sceKernelRegisterSubIntrHandler(PSP_VBLANK_INT, 0, vblankHandler, NULL);
	sceKernelEnableSubIntr(PSP_VBLANK_INT, 0);

	checkpointNext("Waiting for the same vblank:");
	static const int mixed[] = { 0x30, 0x10, 0x18 };
	static const int equal[] = { 0x20, 0x20, 0x20 };
	static const int better[] = { 0x18, 0x10, 0x18 };
	run("Mixed", mixed, 3);
	run("Equal to main", equal, 3);
	run("Better than main", better, 3);

	sceKernelDisableSubIntr(PSP_VBLANK_INT, 0);
	sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, 0);
	flushschedf();
	return 0;
}
