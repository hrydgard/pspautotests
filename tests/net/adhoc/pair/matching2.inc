// More matching between two consoles: a host rejecting a request, a client cancelling one, the hello
// data changing, a full host, the host kicking its client, and a P2P session. Built as
// matching2_host and matching2_client.

#include "pairmatching.h"

#define MATCHING_PORT 0x3200

static inline int waitHelloWith(const char *opt, int from, int timeoutMs) {
	for (int i = 0; i < timeoutMs / 10; ++i) {
		for (int j = from; j < mNumEvents; ++j) {
			if (mEvents[j].event == 1 && strcmp((const char *)mEvents[j].opt, opt) == 0) {
				return j;
			}
		}
		sceKernelDelayThread(10000);
	}
	return -1;
}

static inline int countEvents(int event, int from) {
	int count = 0;
	for (int i = from; i < mNumEvents; ++i) {
		if (mEvents[i].event == event) {
			count++;
		}
	}
	return count;
}

static void hostClient() {
	int start;
#if HOST
	// maxnum 2: room for exactly one client.
	int id = sceNetAdhocMatchingCreate(1, 2, MATCHING_PORT, 2048, 200000, 500000, 5, 200000, &matchingHandler);
	printf("  Start: %08x\n", sceNetAdhocMatchingStart(id, 0x30, 0x2000, 0x30, 0x2000, 4, "OPT1"));
	pairSync(1);
	printf("  SetHelloOpt: %08x\n", sceNetAdhocMatchingSetHelloOpt(id, 4, "OPT2"));
	// The client joins as soon as its pairSync returns, which can be before ours does.
	start = mNumEvents;
	pairSync(2);

	printf("Rejecting:\n");
	printf("  Got REQUEST: %s\n", waitMatching(2, start, 5000) >= 0 ? "yes" : "no");
	printf("  CancelTargetWithOpt: %08x\n", sceNetAdhocMatchingCancelTargetWithOpt(id, &peerMac, 4, "NOPE"));
	pairSync(3);
	printMatchingEvents(start);

	printf("Client cancels:\n");
	start = mNumEvents;
	printf("  Got REQUEST: %s\n", waitMatching(2, start, 5000) >= 0 ? "yes" : "no");
	pairSync(4);
	printf("  Got CANCEL: %s\n", waitMatching(5, start, 3000) >= 0 ? "yes" : "no");
	printf("  SelectTarget on the cancelled request: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 0, NULL));
	pairSync(5);
	printMatchingEvents(start);

	printf("Accepting:\n");
	start = mNumEvents;
	printf("  Got REQUEST: %s\n", waitMatching(2, start, 5000) >= 0 ? "yes" : "no");
	printf("  SelectTarget: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 0, NULL));
	printf("  Got ESTABLISHED: %s\n", waitMatching(7, start, 5000) >= 0 ? "yes" : "no");
	pairSync(6);
	// The client counts hellos now that the host is full.
	pairSync(7);
	printMatchingEvents(start);

	printf("Kicking:\n");
	start = mNumEvents;
	printf("  CancelTarget: %08x\n", sceNetAdhocMatchingCancelTarget(id, &peerMac));
	sceKernelDelayThread(500000);
	int size = 0;
	sceNetAdhocMatchingGetMembers(id, &size, NULL);
	printf("  Members afterwards: %d\n", size / 12);
	pairSync(8);
	printMatchingEvents(start);
#else
	int id = sceNetAdhocMatchingCreate(2, 2, MATCHING_PORT, 2048, 200000, 500000, 5, 200000, &matchingHandler);
	printf("  Start: %08x\n", sceNetAdhocMatchingStart(id, 0x30, 0x2000, 0x30, 0x2000, 0, NULL));
	printf("  Got HELLO 'OPT1': %s\n", waitHelloWith("OPT1", 0, 5000) >= 0 ? "yes" : "no");
	pairSync(1);
	printf("  Got HELLO 'OPT2': %s\n", waitHelloWith("OPT2", 0, 5000) >= 0 ? "yes" : "no");
	pairSync(2);

	printf("Rejecting:\n");
	start = mNumEvents;
	printf("  SelectTarget: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 5, "JOIN1"));
	// The host waits up to 5 seconds for this request before it cancels, so wait longer than that.
	printf("  Got DENY: %s\n", waitMatching(4, start, 8000) >= 0 ? "yes" : "no");
	pairSync(3);
	printMatchingEvents(start);

	printf("Client cancels:\n");
	start = mNumEvents;
	waitMatching(1, start, 3000);
	printf("  SelectTarget: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 5, "JOIN2"));
	pairSync(4);
	printf("  CancelTarget: %08x\n", sceNetAdhocMatchingCancelTarget(id, &peerMac));
	pairSync(5);
	printMatchingEvents(start);

	printf("Accepting:\n");
	start = mNumEvents;
	waitMatching(1, start, 3000);
	printf("  SelectTarget: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 5, "JOIN3"));
	printf("  Got ESTABLISHED: %s\n", waitMatching(7, start, 5000) >= 0 ? "yes" : "no");
	pairSync(6);
	int fullStart = mNumEvents;
	// A full host shouldn't advertise. Hellos come every 200ms, so 2 seconds is plenty.
	sceKernelDelayThread(2000000);
	printf("  Hellos while the host is full: %s\n", countEvents(1, fullStart) == 0 ? "none" : "some");
	// The host kicks as soon as its pairSync returns, which can be before ours does.
	int kickStart = mNumEvents;
	pairSync(7);
	printMatchingEventsUntil(start, kickStart);

	printf("Kicking:\n");
	start = kickStart;
	printf("  Got LEAVE: %s\n", waitMatching(3, start, 5000) >= 0 ? "yes" : "no");
	int size = 0;
	sceNetAdhocMatchingGetMembers(id, &size, NULL);
	printf("  Members afterwards: %d\n", size / 12);
	pairSync(8);
	printMatchingEvents(start);
#endif
	// Don't let one side's Stop (and its BYE) reach the other before it has looked.
	pairSync(9);
	sceNetAdhocMatchingStop(id);
	sceNetAdhocMatchingDelete(id);
}

static void p2p() {
	int id = sceNetAdhocMatchingCreate(3, 2, MATCHING_PORT + 1, 2048, 200000, 500000, 5, 200000, &matchingHandler);
	printf("  Create: %s\n", id > 0 ? "ok" : "failed");
	int start = mNumEvents;
#if HOST
	printf("  Start: %08x\n", sceNetAdhocMatchingStart(id, 0x30, 0x2000, 0x30, 0x2000, 4, "HP2P"));
	pairSync(10);
	printf("  Got REQUEST: %s\n", waitMatching(2, start, 5000) >= 0 ? "yes" : "no");
	printf("  SelectTarget: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 0, NULL));
	printf("  Got ESTABLISHED: %s\n", waitMatching(7, start, 5000) >= 0 ? "yes" : "no");
	pairSync(11);
	printf("  SendData: %08x\n", sceNetAdhocMatchingSendData(id, &peerMac, 4, "PING"));
	printf("  Got DATA_ACK: %s\n", waitMatching(12, start, 5000) >= 0 ? "yes" : "no");
	printf("  AbortSendData with nothing pending: %08x\n", sceNetAdhocMatchingAbortSendData(id, &peerMac));
	int ackIndex = waitMatching(12, start, 0);
	printf("  SendData: %08x\n", sceNetAdhocMatchingSendData(id, &peerMac, 4, "PONG"));
	printf("  AbortSendData at once: %08x\n", sceNetAdhocMatchingAbortSendData(id, &peerMac));
	printf("  SendData after aborting: %08x\n", sceNetAdhocMatchingSendData(id, &peerMac, 4, "PANG"));
	printf("  Another DATA_ACK: %s\n", waitMatching(12, ackIndex + 1, 3000) >= 0 ? "yes" : "no");
	printf("  AbortSendData, unknown peer: %08x\n", sceNetAdhocMatchingAbortSendData(id, &MAC_OTHER));
	pairSync(12);
#else
	printf("  Start: %08x\n", sceNetAdhocMatchingStart(id, 0x30, 0x2000, 0x30, 0x2000, 4, "CP2P"));
	pairSync(10);
	printf("  Got HELLO: %s\n", waitMatching(1, start, 5000) >= 0 ? "yes" : "no");
	printf("  SelectTarget: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 0, NULL));
	printf("  Got ESTABLISHED: %s\n", waitMatching(7, start, 5000) >= 0 ? "yes" : "no");
	pairSync(11);
	printf("  Got DATA: %s\n", waitMatching(11, start, 5000) >= 0 ? "yes" : "no");
	pairSync(12);
	// What arrived of PONG and PANG.
	printf("  Data received:");
	for (int i = start; i < mNumEvents; ++i) {
		if (mEvents[i].event == 11) {
			printf(" '%s'", mEvents[i].opt);
		}
	}
	printf("\n");
#endif
	int size = 0;
	sceNetAdhocMatchingGetMembers(id, &size, NULL);
	printf("  Members: %d\n", size / 12);
	// Both sides advertise in P2P mode, so whether a hello arrives before the request varies; leave them out.
	printf("  Events, without hellos:");
	for (int i = start; i < mNumEvents; ++i) {
		if (mEvents[i].event != 1) {
			printf(" %d", mEvents[i].event);
		}
	}
	printf("\n");
	// Don't let one side's Stop (and its BYE) reach the other before it has looked.
	pairSync(13);
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
	pairSync(9);
	printf("\nP2P:\n");
	p2p();
	sceNetAdhocMatchingTerm();
	pairShutdown();
	return 0;
}
