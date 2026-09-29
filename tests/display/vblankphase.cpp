#include <common.h>
#include <pspdisplay.h>
#include <pspthreadman.h>
#include <pspintrman.h>

extern "C" {
	int sceDisplayGetCurrentHcount();
}

// Where the vblank interrupt falls against the display's counters, and how much CPU it takes from a
// thread that keeps running, with and without a handler. A thread polls the counters through a
// vblank; the one long gap in its polling is the interrupt.
//
// Measured: the interrupt comes at the end of line 285 (the last; 286 lines a frame), with the
// vblank flag already on. hcount wraps to 0 and vcount steps inside it. It takes ~66us, or ~78us
// with a handler, which runs ~29us in and reads line 0. A thread waiting on the vblank gets the CPU
// ~81us after the interrupt (~91us with a handler), a little later with others waiting too, and
// they follow ~12us apart. The vblank ends ~818us after the interrupt.

struct Sample {
	u32 t;
	int h;
	int v;
	int vb;
};

static volatile u32 intrAt;
static volatile int intrH, intrVb;

static void vblankHandler(int no, void *arg) {
	intrAt = sceKernelGetSystemTimeLow();
	intrH = sceDisplayGetCurrentHcount();
	intrVb = sceDisplayIsVblank();
}

static Sample changes[512];
static int numChanges;
static Sample beforeGap;
static u32 gapLen;

// Polls from the middle of a frame until well past the next vblank, keeping every change of the
// counters and the longest gap between two polls.
static void poll() {
	sceDisplayWaitVblankStart();
	u32 start = sceKernelGetSystemTimeLow();
	while (sceKernelGetSystemTimeLow() - start < 8000) {
		continue;
	}

	numChanges = 0;
	gapLen = 0;
	Sample last = { sceKernelGetSystemTimeLow(), sceDisplayGetCurrentHcount(), (int)sceDisplayGetVcount(), sceDisplayIsVblank() };
	start = last.t;
	while (last.t - start < 12000) {
		Sample s;
		s.t = sceKernelGetSystemTimeLow();
		s.h = sceDisplayGetCurrentHcount();
		s.v = sceDisplayGetVcount();
		s.vb = sceDisplayIsVblank();
		if (s.t - last.t > gapLen) {
			gapLen = s.t - last.t;
			beforeGap = last;
		}
		if ((s.v != last.v || s.vb != last.vb || s.h != last.h) && numChanges < 512) {
			changes[numChanges++] = s;
		}
		last = s;
	}
}

struct Tally {
	int gapTotal;
	int lastLine285;
	int endInRange;
	int handlerLine0, handlerInRange;
};

static void measure(Tally &tally, bool withHandler) {
	poll();

	tally.gapTotal += gapLen;

	int lastLine = -1;
	u32 endAt = 0;
	bool seenVblank = false;
	for (int i = 0; i < numChanges; ++i) {
		if (changes[i].vb) {
			seenVblank = true;
		} else if (!seenVblank) {
			lastLine = changes[i].h;
		} else if (endAt == 0) {
			endAt = changes[i].t;
		}
	}
	if (lastLine == 285) {
		tally.lastLine285++;
	}
	int endFromIntr = (int)(endAt - beforeGap.t);
	if (endFromIntr >= 790 && endFromIntr <= 850) {
		tally.endInRange++;
	}

	if (withHandler) {
		if (intrH == 0 && intrVb) {
			tally.handlerLine0++;
		}
		int handlerFromIntr = (int)(intrAt - beforeGap.t);
		if (handlerFromIntr >= 15 && handlerFromIntr <= 45) {
			tally.handlerInRange++;
		}
	}
}

static void run(const char *title, bool withHandler) {
	static const int COUNT = 8;
	Tally tally = {};
	for (int i = 0; i < COUNT; ++i) {
		measure(tally, withHandler);
	}
	checkpoint("%s:", title);
	// A few us either way from run to run on hardware, so only the average, roughly.
	const int avg = tally.gapTotal / COUNT;
	checkpoint("  CPU taken on average: %s", avg < 58 ? "under 58us" : avg < 70 ? "58-69us" : avg < 86 ? "70-85us" : "86us or more");
	checkpoint("  Last line before the wrap is 285: %d of %d", tally.lastLine285, COUNT);
	checkpoint("  Vblank ends 790-850us after the interrupt: %d of %d", tally.endInRange, COUNT);
	if (withHandler) {
		checkpoint("  Handler reads line 0 in vblank: %d of %d", tally.handlerLine0, COUNT);
		checkpoint("  Handler runs 15-45us into the interrupt: %d of %d", tally.handlerInRange, COUNT);
	}
}

static volatile u32 waiterAt[4];
static volatile int waiterGo;

static int waiterFunc(SceSize argc, void *argp) {
	int index = *(int *)argp;
	while (waiterGo) {
		sceDisplayWaitVblankStart();
		waiterAt[index] = sceKernelGetSystemTimeLow();
	}
	return 0;
}

// With higher priority threads waiting on the vblank: when the first and last get the CPU after the
// interrupt.
static void runWaiter(const char *title, int count, int firstMin, int firstMax) {
	static const int RUNS = 6;
	waiterGo = 1;
	SceUID threads[4];
	for (int i = 0; i < count; ++i) {
		threads[i] = sceKernelCreateThread("waiter", &waiterFunc, 0x18, 0x1000, 0, NULL);
		sceKernelStartThread(threads[i], sizeof(i), &i);
	}
	int firstInRange = 0, spreadInRange = 0;
	for (int n = 0; n < RUNS; ++n) {
		poll();
		int first = 100000, last = -100000;
		for (int i = 0; i < count; ++i) {
			int at = (int)(waiterAt[i] - beforeGap.t);
			first = at < first ? at : first;
			last = at > last ? at : last;
		}
		if (first >= firstMin && first <= firstMax) {
			firstInRange++;
		}
		// Each goes back to waiting before the next gets the CPU.
		if (last - first >= 9 * (count - 1) && last - first <= 15 * (count - 1)) {
			spreadInRange++;
		}
	}
	waiterGo = 0;
	for (int i = 0; i < count; ++i) {
		sceKernelWaitThreadEnd(threads[i], NULL);
		sceKernelDeleteThread(threads[i]);
	}
	checkpoint("%s, %d waiting: first runs %d-%dus after the interrupt: %d of %d", title, count, firstMin, firstMax, firstInRange, RUNS);
	if (count > 1) {
		checkpoint("%s, %d waiting: the rest follow %d-%dus apart: %d of %d", title, count, 9, 15, spreadInRange, RUNS);
	}
}

extern "C" int main(int argc, char *argv[]) {
	runWaiter("No handler", 1, 75, 90);
	runWaiter("No handler", 3, 78, 95);
	run("No handler", false);

	sceKernelRegisterSubIntrHandler(PSP_VBLANK_INT, 0, (void *)vblankHandler, NULL);
	sceKernelEnableSubIntr(PSP_VBLANK_INT, 0);
	runWaiter("Handler", 1, 85, 100);
	runWaiter("Handler", 3, 88, 105);
	run("Handler", true);
	sceKernelDisableSubIntr(PSP_VBLANK_INT, 0);
	sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, 0);

	return 0;
}
