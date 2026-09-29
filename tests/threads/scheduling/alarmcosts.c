#include <common.h>
#include <pspthreadman.h>

// What an alarm costs: the call that sets it, when the handler runs (it never goes off sooner than
// about 215us after being set), how long until a thread the handler wakes gets the CPU, and how
// much time a thread that keeps running loses to a handler that wakes nothing. The shortest of a
// few tries, checked against ranges around what hardware gives: 42-45us for the call, 256-258us
// and 1040-1042us for the handler, 49-51us to the woken thread, and ~72us lost.

static volatile u32 handlerAt;
static SceUID self;

static SceUInt wakeHandler(void *common) {
	handlerAt = sceKernelGetSystemTimeLow();
	sceKernelWakeupThread(self);
	return 0;
}

static SceUInt quietHandler(void *common) {
	handlerAt = sceKernelGetSystemTimeLow();
	return 0;
}

static const char *range(u32 us, u32 lo, u32 hi) {
	static char buf[4][32];
	static int n = 0;
	char *b = buf[n++ & 3];
	if (us < lo) {
		sprintf(b, "under %dus", (int)lo);
	} else if (us > hi) {
		sprintf(b, "over %dus", (int)hi);
	} else {
		sprintf(b, "%d-%dus", (int)lo, (int)hi);
	}
	return b;
}

int main(int argc, char *argv[]) {
	self = sceKernelGetThreadId();

	checkpointNext("Alarm, main sleeping until the handler wakes it:");
	static const u32 lengths[] = { 0, 100, 200, 1000, 10000 };
	for (int i = 0; i < (int)(sizeof(lengths) / sizeof(lengths[0])); ++i) {
		u32 setCost = 0xFFFFFFFF, handler = 0xFFFFFFFF, wake = 0xFFFFFFFF;
		for (int k = 0; k < 4; ++k) {
			u32 start = sceKernelGetSystemTimeLow();
			sceKernelSetAlarm(lengths[i], &wakeHandler, NULL);
			u32 set = sceKernelGetSystemTimeLow();
			sceKernelSleepThread();
			u32 woke = sceKernelGetSystemTimeLow();
			setCost = set - start < setCost ? set - start : setCost;
			handler = handlerAt - start < handler ? handlerAt - start : handler;
			wake = woke - handlerAt < wake ? woke - handlerAt : wake;
		}
		const u32 handlerLo = lengths[i] < 215 ? 245 : lengths[i] + 30;
		schedf("  %5d: sceKernelSetAlarm %s, handler at %s, main %s after that\n", (int)lengths[i], range(setCost, 35, 50), range(handler, handlerLo, handlerLo + 25), range(wake, 40, 60));
	}

	checkpointNext("Alarm, main running:");
	{
		// The gap in main's polling that the handler falls in. The longest gap overall could be a
		// vblank instead.
		u32 lost = 0;
		for (int k = 0; k < 4; ++k) {
			handlerAt = 0;
			u32 start = sceKernelGetSystemTimeLow();
			sceKernelSetAlarm(1000, &quietHandler, NULL);
			u32 last = sceKernelGetSystemTimeLow();
			u32 gap = 0;
			while (sceKernelGetSystemTimeLow() - start < 2000) {
				u32 now = sceKernelGetSystemTimeLow();
				const u32 at = handlerAt;
				if (gap == 0 && at != 0 && at - last <= now - last) {
					gap = now - last;
				}
				last = now;
			}
			lost = k == 0 || gap < lost ? gap : lost;
		}
		schedf("  Main lost %s around the handler\n", range(lost, 60, 85));
	}

	flushschedf();
	return 0;
}
