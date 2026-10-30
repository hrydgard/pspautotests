// Giving up on a group join: sceNetAdhocctlDisconnect right after sceNetAdhocctlCreate or Connect,
// before the CONNECT event. What the calls return, and every event and state change for 2 s after.

#include "adhoc.h"

static int lastStateSeen = -2;

static void printStateIfChanged() {
	int state = -1;
	sceNetAdhocctlGetState(&state);
	if (state != lastStateSeen) {
		printf("    state %d\n", state);
		lastStateSeen = state;
	}
}

// Prints the events from start on, and the state each time it changes, for ms milliseconds.
static void watch(int start, int ms) {
	int printed = start;
	for (int i = 0; i < ms; ++i) {
		while (printed < eventCount && printed < 16) {
			printf("    event %d(%08x)\n", eventLog[printed][0], eventLog[printed][1]);
			printed++;
		}
		printStateIfChanged();
		sceKernelDelayThread(1000);
	}
	while (printed < eventCount && printed < 16) {
		printf("    event %d(%08x)\n", eventLog[printed][0], eventLog[printed][1]);
		printed++;
	}
	printStateIfChanged();
}

static void printPeers() {
	AdhocctlPeerInfo peers[4];
	int size = sizeof(peers);
	int result = sceNetAdhocctlGetPeerList(&size, peers);
	printf("  Peer list: %08x, %d peers\n", result, size / (int)sizeof(AdhocctlPeerInfo));
}

// Starts again from scratch: out of any group, with the event log empty. Disconnect is BUSY while a
// join is still going on, so it's retried until that has finished.
static void reset() {
	for (int i = 0; i < 200; ++i) {
		int start = eventCount;
		int result = sceNetAdhocctlDisconnect();
		if (result == 0) {
			waitEvent(start, 3000);
			break;
		}
		sceKernelDelayThread(100000);
	}
	sceKernelDelayThread(200000);
	eventCount = 0;
	lastStateSeen = -2;
}

static int join(int create, const char *group) {
	return create ? sceNetAdhocctlCreate(group) : sceNetAdhocctlConnect(group);
}

static void testAbandon(const char *title, int create, int delayUs, int watchMs) {
	reset();
	printf("%s:\n", title);
	int result = join(create, "ABANDON");
	if (delayUs > 0) {
		sceKernelDelayThread(delayUs);
	}
	int state = -1;
	sceNetAdhocctlGetState(&state);
	int disconnect = sceNetAdhocctlDisconnect();
	printf("  %s: %08x, state then %d, Disconnect: %08x\n", create ? "Create" : "Connect", result, state, disconnect);
	watch(0, watchMs);
	printPeers();
}

static void testAbandonAndRejoin(const char *title) {
	reset();
	printf("%s:\n", title);
	int result = sceNetAdhocctlCreate("ABANDON");
	int disconnect = sceNetAdhocctlDisconnect();
	int again = sceNetAdhocctlCreate("REJOIN");
	printf("  Create: %08x, Disconnect: %08x, Create again: %08x\n", result, disconnect, again);
	watch(0, 2000);
	AdhocctlParameter param;
	memset(&param, 0, sizeof(param));
	int paramResult = sceNetAdhocctlGetParameter(&param);
	char name[9] = {};
	memcpy(name, param.group, 8);
	printf("  GetParameter: %08x, group %s\n", paramResult, name);
	printPeers();
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

	printf("\n");
	testAbandon("Create, then Disconnect at once", 1, 0, 2000);
	// Connecting to a group nobody hosts takes longer.
	testAbandon("Connect, then Disconnect at once", 0, 0, 10000);
	testAbandon("Create, then Disconnect 1 ms later", 1, 1000, 2000);
	testAbandon("Create, then Disconnect 20 ms later", 1, 20000, 2000);
	testAbandonAndRejoin("Create, Disconnect, Create another at once");

	reset();
	printf("\nDone, state:\n");
	printStateIfChanged();

	adhocShutdown();
	return 0;
}
