// sceNetAdhocctl on one console: init, handlers, group names, and creating, leaving, scanning for
// and connecting to a group with nobody else around.

#include "adhoc.h"

#define EXTRA_HANDLER(n) static void extraHandler##n(int event, int error, void *arg) {}
EXTRA_HANDLER(0) EXTRA_HANDLER(1) EXTRA_HANDLER(2) EXTRA_HANDLER(3) EXTRA_HANDLER(4)
EXTRA_HANDLER(5) EXTRA_HANDLER(6) EXTRA_HANDLER(7) EXTRA_HANDLER(8) EXTRA_HANDLER(9)
static AdhocctlHandler extraHandlers[] = {
	extraHandler0, extraHandler1, extraHandler2, extraHandler3, extraHandler4,
	extraHandler5, extraHandler6, extraHandler7, extraHandler8, extraHandler9,
};

static void printEventsSince(int start) {
	printf("    events:");
	for (int i = start; i < eventCount && i < 16; ++i) {
		printf(" %d(%08x)", eventLog[i][0], eventLog[i][1]);
	}
	printf("\n");
}

static void printState(const char *title) {
	int state = -1;
	int result = sceNetAdhocctlGetState(&state);
	printf("%s: %08x, state %d\n", title, result, state);
}

static void printParameter(const char *title) {
	AdhocctlParameter param;
	memset(&param, 0xCC, sizeof(param));
	int result = sceNetAdhocctlGetParameter(&param);
	if (result < 0) {
		printf("%s: %08x\n", title, result);
		return;
	}
	char group[9] = {};
	memcpy(group, param.group, 8);
	int channelOk = param.channel == 1 || param.channel == 6 || param.channel == 11;
	printf("%s: %08x, channel %s, group '%s', nickname %s, bssid %s", title, result, channelOk ? "1/6/11" : "unexpected",
		group, param.nickname[0] != 0 && param.nickname[0] != (char)0xCC ? "set" : "empty", macName(&param.bssid));
	if (strcmp(macName(&param.bssid), "unexpected") == 0) {
		// Print the two low bits of the first byte: multicast, and locally administered.
		printf(" (low bits %d)", param.bssid.data[0] & 3);
	}
	printf("\n");
}

static void printPeers(const char *title) {
	int size = 0x1234;
	int result = sceNetAdhocctlGetPeerList(&size, NULL);
	printf("%s: %08x, size %d\n", title, result, size);
}

static void printScanInfo(const char *title) {
	int size = 0x1234;
	int result = sceNetAdhocctlGetScanInfo(&size, NULL);
	printf("%s: %08x, size %d\n", title, result, size);
}

// Runs an asynchronous call and waits for its event.
static int runAndWait(const char *title, int result) {
	int start = eventCount;
	int stateAfter = -1;
	sceNetAdhocctlGetState(&stateAfter);
	int event = result < 0 ? -1 : waitEvent(start, 5000);
	int state = -1;
	sceNetAdhocctlGetState(&state);
	printf("%s: %08x, state right after %d, event %d, state %d\n", title, result, stateAfter, event, state);
	if (result >= 0) {
		printEventsSince(start);
	}
	return event;
}

// Tries a group name that should be rejected; if it is accepted, leaves the group again.
static void testName(const char *title, const char *name) {
	int start = eventCount;
	int result = sceNetAdhocctlCreate(name);
	printf("%s: %08x\n", title, result);
	if (result >= 0) {
		waitEvent(start, 5000);
		start = eventCount;
		sceNetAdhocctlDisconnect();
		waitEvent(start, 5000);
	}
}

static void testInit(const char *title, int stack, int prio, const AdhocId *id) {
	int result = sceNetAdhocctlInit(stack, prio, id);
	int state = -1;
	int stateResult = sceNetAdhocctlGetState(&state);
	printf("%s: %08x, GetState %08x\n", title, result, stateResult);
	if (result >= 0) {
		sceNetAdhocctlTerm();
		// Term returns before adhocctl's thread has stopped.
		sceKernelDelayThread(100000);
	}
}

int main(int argc, char *argv[]) {
	if (!adhocStartup()) {
		return 1;
	}
	printf("sceNetAdhocInit: %08x\n", sceNetAdhocInit());

	printf("\nBefore init:\n");
	printState("  GetState");
	printf("  AddHandler: %08x\n", sceNetAdhocctlAddHandler(&adhocctlHandler, NULL));
	printf("  Create: %08x\n", sceNetAdhocctlCreate("TEST"));
	printf("  Term: %08x\n", sceNetAdhocctlTerm());

	printf("\nInit:\n");
	AdhocId badType = TEST_ADHOC_ID;
	badType.type = 3;
	AdhocId lowercase = TEST_ADHOC_ID;
	lowercase.product[0] = 't';
	testInit("  Stack 0", 0, 0x30, &TEST_ADHOC_ID);
	testInit("  Stack 0x400", 0x400, 0x30, &TEST_ADHOC_ID);
	testInit("  Stack 0xBFF", 0xBFF, 0x30, &TEST_ADHOC_ID);
	testInit("  Stack 0xC00", 0xC00, 0x30, &TEST_ADHOC_ID);
	testInit("  Priority 0", 0x2000, 0, &TEST_ADHOC_ID);
	testInit("  Priority 0x80", 0x2000, 0x80, &TEST_ADHOC_ID);
	testInit("  NULL id", 0x2000, 0x30, NULL);
	testInit("  Type 3", 0x2000, 0x30, &badType);
	testInit("  Lowercase product", 0x2000, 0x30, &lowercase);
	if (!adhocctlStartup()) {
		adhocShutdown();
		return 1;
	}
	printState("  GetState");

	AdhocId id;
	memset(&id, 0xCC, sizeof(id));
	int result = sceNetAdhocctlGetAdhocId(&id);
	printf("  GetAdhocId: %08x, %s\n", result, memcmp(&id, &TEST_ADHOC_ID, 13) == 0 ? "matches" : "differs");
	AdhocMac mac;
	printf("  sceNetGetLocalEtherAddr: %08x\n", sceNetGetLocalEtherAddr(&mac));

	printf("\nDisconnected:\n");
	printState("  GetState");
	printParameter("  GetParameter");
	printPeers("  GetPeerList");
	printScanInfo("  GetScanInfo");
	int disconnectStart = eventCount;
	printf("  Disconnect: %08x\n", sceNetAdhocctlDisconnect());
	// Whether a Disconnect that has nothing to leave still notifies the handlers.
	sceKernelDelayThread(500000);
	printEventsSince(disconnectStart);

	printf("\nHandlers:\n");
	printf("  NULL: %08x\n", sceNetAdhocctlAddHandler(NULL, NULL));
	handlerId = sceNetAdhocctlAddHandler(&adhocctlHandler, NULL);
	printf("  First: %08x\n", handlerId);
	int sameAgain = sceNetAdhocctlAddHandler(&adhocctlHandler, NULL);
	printf("  Same again: %08x\n", sameAgain);
	int extra[10];
	int count = 0;
	for (count = 0; count < 10; ++count) {
		extra[count] = sceNetAdhocctlAddHandler(extraHandlers[count], NULL);
		if (extra[count] < 0) {
			break;
		}
	}
	printf("  Added %d more, then %08x\n", count, count < 10 ? extra[count] : 0);
	for (int i = 0; i < count; ++i) {
		sceNetAdhocctlDelHandler(extra[i]);
	}
	if (sameAgain >= 0) {
		printf("  Delete the duplicate: %08x\n", sceNetAdhocctlDelHandler(sameAgain));
	}
	printf("  Delete invalid: %08x\n", sceNetAdhocctlDelHandler(100));
	printf("  Delete -1: %08x\n", sceNetAdhocctlDelHandler(-1));

	printf("\nGroup names:\n");
	testName("  NULL", NULL);
	testName("  Dash", "BAD-NAME");
	testName("  Space", "BAD NAME");
	testName("  Underscore", "BAD_NAME");

	printf("\nCreate:\n");
	int event = runAndWait("  Create TESTGRP1", sceNetAdhocctlCreate("TESTGRP1"));
	if (event == 1) {
		printParameter("  GetParameter");
		printf("  sceNetGetLocalEtherAddr: %08x, %s\n", sceNetGetLocalEtherAddr(&mac), macName(&mac));
		printPeers("  GetPeerList");
		AdhocctlPeerInfo info;
		printf("  GetPeerInfo local: %08x\n", sceNetAdhocctlGetPeerInfo(&localMac, sizeof(info), &info));
		printf("  GetPeerInfo other: %08x\n", sceNetAdhocctlGetPeerInfo(&MAC_OTHER, sizeof(info), &info));
		printScanInfo("  GetScanInfo");

		printf("\nWhile connected:\n");
		int start = eventCount;
		printf("  Create: %08x\n", sceNetAdhocctlCreate("TESTGRP2"));
		printf("  Connect: %08x\n", sceNetAdhocctlConnect("TESTGRP2"));
		printf("  Scan: %08x\n", sceNetAdhocctlScan());
		sceKernelDelayThread(100000);
		printEventsSince(start);
		printState("  GetState");

		printf("\nDisconnect:\n");
		runAndWait("  Disconnect", sceNetAdhocctlDisconnect());
		printParameter("  GetParameter");
		printPeers("  GetPeerList");
		int againStart = eventCount;
		printf("  Disconnect again: %08x\n", sceNetAdhocctlDisconnect());
		sceKernelDelayThread(500000);
		printEventsSince(againStart);
	}

	printf("\nScan:\n");
	event = runAndWait("  Scan", sceNetAdhocctlScan());
	printScanInfo("  GetScanInfo");

	printf("\nConnect:\n");
	event = runAndWait("  Connect TESTGRP3", sceNetAdhocctlConnect("TESTGRP3"));
	if (event == 1) {
		printParameter("  GetParameter");
		runAndWait("  Disconnect", sceNetAdhocctlDisconnect());
	}

	printf("\nInit again:\n");
	// This fails, and also leaves adhocctl uninitialized, in a way that a new Init can't fix.
	testInit("  Again", 0x2000, 0x30, &TEST_ADHOC_ID);
	printState("  GetState");
	testInit("  And again", 0x2000, 0x30, &TEST_ADHOC_ID);

	printf("\nTerm:\n");
	printf("  DelHandler: %08x\n", sceNetAdhocctlDelHandler(handlerId));
	printf("  Term: %08x\n", sceNetAdhocctlTerm());
	printf("  Term again: %08x\n", sceNetAdhocctlTerm());
	printState("  GetState after term");

	adhocShutdown();
	return 0;
}
