// sceNetAdhocctl requests made right after sceNetAdhocctlInit, before anything else has had a chance to
// run, on one console.

#include "adhoc.h"

static void printEventsSince(int start) {
	printf("    events:");
	for (int i = start; i < eventCount && i < 16; ++i) {
		printf(" %d(%08x)", eventLog[i][0], eventLog[i][1]);
	}
	printf("\n");
}

static int init(const char *title) {
	u64 start = sceKernelGetSystemTimeWide();
	int result = sceNetAdhocctlInit(0x2000, 0x30, &TEST_ADHOC_ID);
	u64 took = sceKernelGetSystemTimeWide() - start;
	handlerId = result < 0 ? -1 : sceNetAdhocctlAddHandler(&adhocctlHandler, NULL);
	printf("%s\n  Init: %08x, within 50 ms: %s, AddHandler: %s\n", title, result, took < 50000 ? "yes" : "no", handlerId >= 0 ? "ok" : "failed");
	return result >= 0;
}

static void finish() {
	int state = -1;
	sceNetAdhocctlGetState(&state);
	if (state != 0) {
		int start = eventCount;
		sceNetAdhocctlDisconnect();
		waitEvent(start, 5000);
	}
	sceNetAdhocctlTerm();
	// Term returns before adhocctl's thread has stopped.
	sceKernelDelayThread(100000);
}

// Prints a request's result, the state right after it, its event and the state then.
static void report(const char *title, int result, int start) {
	int stateAfter = -1;
	sceNetAdhocctlGetState(&stateAfter);
	int event = result < 0 ? -1 : waitEvent(start, 5000);
	int state = -1;
	sceNetAdhocctlGetState(&state);
	printf("  %s: %08x, state right after %d, event %d, state %d\n", title, result, stateAfter, event, state);
	if (result >= 0) {
		printEventsSince(start);
	}
}

int main(int argc, char *argv[]) {
	if (!adhocStartup()) {
		return 1;
	}
	printf("sceNetAdhocInit: %08x\n", sceNetAdhocInit());

	if (init("Create right after Init:")) {
		int start = eventCount;
		report("Create", sceNetAdhocctlCreate("CTLINIT1"), start);
		finish();
	}

	if (init("Connect right after Init:")) {
		int start = eventCount;
		report("Connect", sceNetAdhocctlConnect("CTLINIT2"), start);
		finish();
	}

	if (init("Scan right after Init:")) {
		int start = eventCount;
		report("Scan", sceNetAdhocctlScan(), start);
		finish();
	}

	if (init("Disconnect right after Init:")) {
		int start = eventCount;
		int result = sceNetAdhocctlDisconnect();
		int event = waitEvent(start, 1000);
		printf("  Disconnect: %08x, event within 1 s %d\n", result, event);
		finish();
	}

	if (init("Queries right after Init:")) {
		int state = -1;
		int result = sceNetAdhocctlGetState(&state);
		printf("  GetState: %08x, state %d\n", result, state);
		int size = 0x1234;
		result = sceNetAdhocctlGetPeerList(&size, NULL);
		printf("  GetPeerList: %08x, size %d\n", result, size);
		size = 0x1234;
		result = sceNetAdhocctlGetScanInfo(&size, NULL);
		printf("  GetScanInfo: %08x, size %d\n", result, size);
		finish();
	}

	if (init("Create, then Scan before its event:")) {
		int start = eventCount;
		int create = sceNetAdhocctlCreate("CTLINIT3");
		int scan = sceNetAdhocctlScan();
		printf("  Create: %08x, Scan right after: %08x\n", create, scan);
		report("(waiting)", create, start);
		finish();
	}

	if (init("Create twice:")) {
		int start = eventCount;
		int first = sceNetAdhocctlCreate("CTLINIT4");
		int second = sceNetAdhocctlCreate("CTLINIT4");
		printf("  Create: %08x, again right after: %08x\n", first, second);
		report("(waiting)", first, start);
		finish();
	}

	adhocShutdown();
	return 0;
}
