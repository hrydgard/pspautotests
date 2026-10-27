// Matching between two consoles around a join that's held or denied: whether a client keeps getting
// the host's hellos while its request is pending, what follows a DENY (more hellos, a TIMEOUT, a
// second request), in host/client and in P2P mode. Built as matching3_host and matching3_client.

#include "pairmatching.h"

#define MATCHING_PORT 0x3300

// Every hello, also the ones matchingHandler folds into the last event.
static volatile int rawHellos = 0;

static void countingHandler(int id, int event, AdhocMac *mac, int optlen, void *opt) {
	if (event == 1) {
		rawHellos++;
	}
	matchingHandler(id, event, mac, optlen, opt);
}

static inline int countSince(int event, int from) {
	int count = 0;
	for (int i = from; i < mNumEvents; ++i) {
		if (mEvents[i].event == event) {
			count++;
		}
	}
	return count;
}

// A held request can be sent again (the client's matching retransmits it), so how many REQUESTs
// arrive varies, and one can arrive after the deny. These leave REQUEST (and hellos) out.
static void printEventsExcept(int from, int skip1, int skip2) {
	printf("  Events, without %s:", skip2 == 1 ? "requests and hellos" : "requests");
	for (int i = from; i < mNumEvents; ++i) {
		if (mEvents[i].event != skip1 && mEvents[i].event != skip2) {
			printf(" %d", mEvents[i].event);
		}
	}
	printf("\n");
}

static void hostClient() {
	int start;
#if HOST
	int id = sceNetAdhocMatchingCreate(1, 4, MATCHING_PORT, 2048, 200000, 500000, 5, 200000, &countingHandler);
	printf("  Start: %08x\n", sceNetAdhocMatchingStart(id, 0x30, 0x2000, 0x30, 0x2000, 2, "H0"));
	pairSync(1);

	printf("Holding a request:\n");
	start = mNumEvents;
	printf("  Got REQUEST: %s\n", waitMatching(2, start, 5000) >= 0 ? "yes" : "no");
	// Different hello data every 200ms, so each one is a new HELLO event on the client.
	static const char *opts[] = { "H1", "H2", "H3", "H4", "H5", "H6", "H7", "H8", "H9", "HA" };
	for (int i = 0; i < 10; ++i) {
		sceNetAdhocMatchingSetHelloOpt(id, 2, opts[i]);
		sceKernelDelayThread(200000);
	}
	pairSync(2);

	printf("Denying it:\n");
	int denyIndex = mNumEvents;
	printf("  CancelTarget: %08x\n", sceNetAdhocMatchingCancelTarget(id, &peerMac));
	pairSync(3);
	// The client watches for 5 seconds.
	pairSync(4);
	printf("  TIMEOUT for the denied client: %s\n", countSince(8, start) > 0 ? "yes" : "no");
	printEventsExcept(start, 2, 2);

	printf("Asking again:\n");
	start = mNumEvents;
	printf("  A REQUEST since the deny: %s\n", waitMatching(2, denyIndex, 5000) >= 0 ? "yes" : "no");
	printf("  SelectTarget: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 0, NULL));
	printf("  Got ESTABLISHED: %s\n", waitMatching(7, start, 5000) >= 0 ? "yes" : "no");
	pairSync(5);
	printEventsExcept(start, 2, 2);
#else
	int id = sceNetAdhocMatchingCreate(2, 4, MATCHING_PORT, 2048, 200000, 500000, 5, 200000, &countingHandler);
	printf("  Start: %08x\n", sceNetAdhocMatchingStart(id, 0x30, 0x2000, 0x30, 0x2000, 0, NULL));
	printf("  Got HELLO: %s\n", waitMatching(1, 0, 5000) >= 0 ? "yes" : "no");
	pairSync(1);

	printf("Holding a request:\n");
	start = mNumEvents;
	printf("  SelectTarget: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 0, NULL));
	int pendingHellos = rawHellos;
	pairSync(2);
	printf("  HELLOs while the request is pending: %s\n", rawHellos == pendingHellos ? "none" : "some");

	printf("Denying it:\n");
	printf("  Got DENY: %s\n", waitMatching(4, start, 5000) >= 0 ? "yes" : "no");
	pairSync(3);
	int afterDeny = rawHellos;
	int afterDenyEvents = mNumEvents;
	// Longer than keepalive_int * init_count (2.5 seconds).
	sceKernelDelayThread(5000000);
	printf("  HELLOs after DENY: %s\n", rawHellos == afterDeny ? "none" : "some");
	printf("  TIMEOUT after DENY: %s\n", countSince(8, afterDenyEvents) > 0 ? "yes" : "no");
	pairSync(4);
	printMatchingEvents(start);

	printf("Asking again:\n");
	start = mNumEvents;
	printf("  SelectTarget: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 0, NULL));
	// The host waits up to 5 seconds for a request first.
	printf("  Got ESTABLISHED: %s\n", waitMatching(7, start, 12000) >= 0 ? "yes" : "no");
	pairSync(5);
	printMatchingEvents(start);
#endif
	// Don't let one side's Stop (and its BYE) reach the other before it has looked.
	pairSync(6);
	sceNetAdhocMatchingStop(id);
	sceNetAdhocMatchingDelete(id);
}

static void p2p() {
	int id = sceNetAdhocMatchingCreate(3, 2, MATCHING_PORT + 1, 2048, 200000, 500000, 5, 200000, &countingHandler);
	printf("  Create: %s\n", id > 0 ? "ok" : "failed");
	int start = mNumEvents;
#if HOST
	printf("  Start: %08x\n", sceNetAdhocMatchingStart(id, 0x30, 0x2000, 0x30, 0x2000, 4, "HP2P"));
	pairSync(10);
	printf("  Got REQUEST: %s\n", waitMatching(2, start, 5000) >= 0 ? "yes" : "no");
	int denyIndex = mNumEvents;
	printf("  CancelTarget: %08x\n", sceNetAdhocMatchingCancelTarget(id, &peerMac));
	pairSync(11);
	// The client watches for 5 seconds.
	pairSync(12);
	printf("  TIMEOUT for the denied peer: %s\n", countSince(8, start) > 0 ? "yes" : "no");
	printEventsExcept(start, 2, 1);

	printf("Asking again:\n");
	start = mNumEvents;
	printf("  A REQUEST since the deny: %s\n", waitMatching(2, denyIndex, 5000) >= 0 ? "yes" : "no");
	printf("  SelectTarget: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 0, NULL));
	printf("  Got ESTABLISHED: %s\n", waitMatching(7, start, 5000) >= 0 ? "yes" : "no");
	pairSync(13);
#else
	printf("  Start: %08x\n", sceNetAdhocMatchingStart(id, 0x30, 0x2000, 0x30, 0x2000, 4, "CP2P"));
	pairSync(10);
	printf("  Got HELLO: %s\n", waitMatching(1, start, 5000) >= 0 ? "yes" : "no");
	printf("  SelectTarget: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 0, NULL));
	printf("  Got DENY: %s\n", waitMatching(4, start, 5000) >= 0 ? "yes" : "no");
	pairSync(11);
	int afterDeny = rawHellos;
	int afterDenyEvents = mNumEvents;
	sceKernelDelayThread(5000000);
	printf("  HELLOs after DENY: %s\n", rawHellos == afterDeny ? "none" : "some");
	printf("  TIMEOUT after DENY: %s\n", countSince(8, afterDenyEvents) > 0 ? "yes" : "no");
	pairSync(12);
	printEventsExcept(start, 2, 1);

	printf("Asking again:\n");
	start = mNumEvents;
	printf("  SelectTarget: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 0, NULL));
	printf("  Got ESTABLISHED: %s\n", waitMatching(7, start, 12000) >= 0 ? "yes" : "no");
	pairSync(13);
#endif
	printEventsExcept(start, 2, 1);
	// Don't let one side's Stop (and its BYE) reach the other before it has looked.
	pairSync(14);
	sceNetAdhocMatchingStop(id);
	sceNetAdhocMatchingDelete(id);
}

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	sceNetAdhocMatchingInit(0x20000);
	printf("\nHost and client:\n");
	hostClient();
	pairSync(7);
	printf("\nP2P:\n");
	p2p();
	sceNetAdhocMatchingTerm();
	pairShutdown();
	return 0;
}
