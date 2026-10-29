// When sceNetAdhocctlGetState reports a scan (state 2), and for how long: the state is polled every
// millisecond from the sceNetAdhocctlScan call until a while after its event. Also the same for
// sceNetAdhocctlCreate (state 1) and sceNetAdhocctlDisconnect. Times are in coarse buckets.

#include "adhoc.h"
#include <psputility_sysparam.h>

static const char *bucket(u64 us) {
	static const int limits[] = { 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000 };
	static const char *names[] = { "under 1 ms", "1-2 ms", "2-5 ms", "5-10 ms", "10-20 ms", "20-50 ms", "50-100 ms", "100-200 ms", "200-500 ms", "500-1000 ms", "1-2 s", "2-5 s" };
	for (int i = 0; i < 12; ++i) {
		if (us < limits[i] * 1000ULL)
			return names[i];
	}
	return "5 s or more";
}

// Calls the request, then prints each state change (and the event) with when it happened.
static int verbose = 0;

static void trace(const char *title, int which) {
	int start = eventCount;
	int lastState = -2;
	int seenEvent = 0;
	u64 stateChangeUs = 0;
	u64 eventAt = 0;
	u64 t0 = sceKernelGetSystemTimeWide();
	int result;
	if (which == 0) {
		result = sceNetAdhocctlScan();
	} else if (which == 1) {
		result = sceNetAdhocctlCreate("SCANST");
	} else {
		result = sceNetAdhocctlDisconnect();
	}
	printf("%s: %08x\n", title, result);
	while (1) {
		u64 now = sceKernelGetSystemTimeWide();
		int state = -1;
		sceNetAdhocctlGetState(&state);
		if (state != lastState) {
			printf("  state %d from %s\n", state, bucket(now - t0));
			if (lastState >= 0) {
				stateChangeUs = now - t0;
			}
			lastState = state;
		}
		if (!seenEvent && eventCount != start) {
			seenEvent = 1;
			eventAt = now;
			printf("  event %d at %s\n", eventLog[start][0], bucket(now - t0));
			if (verbose) {
				printf("  (event at %d ms, last state change at %d ms)\n", (int)((now - t0) / 1000), (int)(stateChangeUs / 1000));
			}
		}
		if (seenEvent && now - eventAt > 200000) {
			break;
		}
		if (now - t0 > 8000000) {
			printf("  no event\n");
			break;
		}
		sceKernelDelayThread(1000);
	}
}

int main(int argc, char *argv[]) {
	if (!adhocStartup()) {
		return 1;
	}
	printf("sceNetAdhocInit: %08x\n", sceNetAdhocInit());
	if (!adhocctlStartup()) {
		adhocShutdown();
		return 1;
	}
	handlerId = sceNetAdhocctlAddHandler(&adhocctlHandler, NULL);
	trace("Scan", 0);
	trace("Scan again", 0);
	trace("Create", 1);
	trace("Disconnect", 2);
	// Exact times, to see the spread. Not compared. They depend on the ad hoc channel setting.
	int channel = -1;
	sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_ADHOC_CHANNEL, &channel);
	printf("\nAd hoc channel setting: %d\n", channel);
	verbose = 1;
	for (int i = 0; i < 4; ++i) {
		trace("Timed scan", 0);
	}
	adhocShutdown();
	return 0;
}
