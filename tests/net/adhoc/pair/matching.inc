// A matching session between two consoles: the client sees the host's hello, asks to join, is
// accepted, they exchange data, and the client cancels. Built as matching_host and matching_client.
// Each side prints the events its callback got, in order, with repeated HELLOs folded into one.

#include "pair.h"

#define MATCHING_PORT 0x3000

typedef struct {
	int event;
	int fromPeer;
	int optlen;
	char opt[16];
} MatchingEvent;

static volatile MatchingEvent events[32];
static volatile int numEvents = 0;
static volatile int helloCount = 0;

static void matchingHandler(int id, int event, AdhocMac *mac, int optlen, void *opt) {
	if (event == 1) {
		helloCount++;
		// Only keep the first of a run of hellos.
		if (numEvents > 0 && events[numEvents - 1].event == 1) {
			return;
		}
	}
	if (numEvents >= 32) {
		return;
	}
	volatile MatchingEvent *e = &events[numEvents];
	e->event = event;
	e->fromPeer = mac != NULL && memcmp(mac, &peerMac, sizeof(AdhocMac)) == 0;
	e->optlen = optlen;
	memset((void *)e->opt, 0, sizeof(e->opt));
	if (opt != NULL && optlen > 0) {
		memcpy((void *)e->opt, opt, optlen < 15 ? optlen : 15);
	}
	numEvents++;
}

// Waits up to timeoutMs for an event of the given type to have arrived.
static int waitFor(int event, int timeoutMs) {
	for (int i = 0; i < timeoutMs / 10; ++i) {
		for (int j = 0; j < numEvents; ++j) {
			if (events[j].event == event) {
				return 1;
			}
		}
		sceKernelDelayThread(10000);
	}
	return 0;
}

static void printEvents() {
	printf("Events:\n");
	for (int i = 0; i < numEvents; ++i) {
		printf("  %d from %s, optlen %d '%s'\n", events[i].event, events[i].fromPeer ? "peer" : "other", events[i].optlen, events[i].opt);
	}
	printf("  (hellos: %s)\n", helloCount == 0 ? "none" : "some");
}

static void printMembers(const char *title, int id) {
	struct Member {
		struct Member *next;
		AdhocMac mac;
		u8 padding[2];
	} members[4];
	int size = sizeof(members);
	int result = sceNetAdhocMatchingGetMembers(id, &size, members);
	printf("%s: %08x, size %d:", title, result, size);
	for (int i = 0; i < size / 12 && i < 4; ++i) {
		printf(" %s", pairMacName(&members[i].mac));
	}
	printf("\n");
}

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}

	printf("\nMatching:\n");
	printf("  Init: %08x\n", sceNetAdhocMatchingInit(0x20000));
#if HOST
	int id = sceNetAdhocMatchingCreate(1, 4, MATCHING_PORT, 2048, 200000, 500000, 5, 200000, &matchingHandler);
#else
	int id = sceNetAdhocMatchingCreate(2, 2, MATCHING_PORT, 2048, 200000, 500000, 5, 200000, &matchingHandler);
#endif
	printf("  Create: %s\n", id > 0 ? "ok" : "failed");
	pairSync(1);
#if HOST
	printf("  Start: %08x\n", sceNetAdhocMatchingStart(id, 0x30, 0x2000, 0x30, 0x2000, 7, "HOSTOPT"));
	printf("  Got REQUEST: %s\n", waitFor(2, 10000) ? "yes" : "no");
	printMembers("  Members before accepting", id);
	printf("  SelectTarget: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 4, "ACPT"));
	printf("  SelectTarget again: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 0, NULL));
	printf("  Got ESTABLISHED: %s\n", waitFor(7, 5000) ? "yes" : "no");
	printMembers("  Members", id);
	pairSync(2);
	printf("  SendData: %08x\n", sceNetAdhocMatchingSendData(id, &peerMac, 5, "HDATA"));
	printf("  SendData again at once: %08x\n", sceNetAdhocMatchingSendData(id, &peerMac, 5, "HDAT2"));
	printf("  Got DATA: %s\n", waitFor(11, 5000) ? "yes" : "no");
	printf("  Got DATA_ACK: %s\n", waitFor(12, 5000) ? "yes" : "no");
	pairSync(3);
	// The client cancels now.
	printf("  Got LEAVE: %s\n", waitFor(3, 10000) ? "yes" : "no");
	printMembers("  Members after the client left", id);
	pairSync(4);
#else
	printf("  Start: %08x\n", sceNetAdhocMatchingStart(id, 0x30, 0x2000, 0x30, 0x2000, 0, NULL));
	printf("  Got HELLO: %s\n", waitFor(1, 10000) ? "yes" : "no");
	printf("  SelectTarget: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 4, "JOIN"));
	printf("  SelectTarget again: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 0, NULL));
	printf("  Got ACCEPT: %s\n", waitFor(6, 10000) ? "yes" : "no");
	printf("  Got ESTABLISHED: %s\n", waitFor(7, 5000) ? "yes" : "no");
	printMembers("  Members", id);
	pairSync(2);
	printf("  Got DATA: %s\n", waitFor(11, 5000) ? "yes" : "no");
	printf("  SendData: %08x\n", sceNetAdhocMatchingSendData(id, &peerMac, 5, "CDATA"));
	printf("  Got DATA_ACK: %s\n", waitFor(12, 5000) ? "yes" : "no");
	pairSync(3);
	printf("  CancelTarget: %08x\n", sceNetAdhocMatchingCancelTarget(id, &peerMac));
	sceKernelDelayThread(500000);
	printMembers("  Members after cancelling", id);
	pairSync(4);
#endif
	// The client stops first, and the host waits for its BYE, so whether it arrives doesn't depend on timing.
#if HOST
	printf("  Got BYE: %s\n", waitFor(10, 5000) ? "yes" : "no");
	pairSync(5);
#else
	sceKernelDelayThread(200000);
#endif
	printf("  Stop: %08x\n", sceNetAdhocMatchingStop(id));
#if !HOST
	pairSync(5);
#endif
	printf("  Delete: %08x\n", sceNetAdhocMatchingDelete(id));
	printf("  Term: %08x\n", sceNetAdhocMatchingTerm());
	printEvents();

	pairShutdown();
	return 0;
}
