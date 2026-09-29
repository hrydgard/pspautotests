#include <common.h>
#include <pspthreadman.h>

// What an alarm costs: the call that sets it, when the handler runs (it never goes off sooner than
// about 215us after being set), how long until a thread the handler wakes gets the CPU, and how
// much time a thread that keeps running loses to a handler that wakes nothing. The shortest of a
// few tries, to 10us.

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

static const char *bucket(u32 us) {
	static char buf[4][16];
	static int n = 0;
	char *b = buf[n++ & 3];
	sprintf(b, "~%dus", (int)((us + 5) / 10 * 10));
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
		schedf("  %5d: sceKernelSetAlarm %s, handler at %s, main %s after that\n", (int)lengths[i], bucket(setCost), bucket(handler), bucket(wake));
	}

	checkpointNext("Alarm, main running:");
	{
		u32 lost = 0;
		handlerAt = 0;
		u32 start = sceKernelGetSystemTimeLow();
		sceKernelSetAlarm(1000, &quietHandler, NULL);
		u32 last = sceKernelGetSystemTimeLow();
		while (sceKernelGetSystemTimeLow() - start < 3000) {
			u32 now = sceKernelGetSystemTimeLow();
			lost = now - last > lost ? now - last : lost;
			last = now;
		}
		schedf("  Handler at %s, main lost %s around it\n", bucket(handlerAt - start), bucket(lost));
	}

	flushschedf();
	return 0;
}
