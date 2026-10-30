// When does accepting a join request in P2P matching raise ESTABLISHED on the accepting console:
// inside sceNetAdhocMatchingSelectTarget, before it returns, or later? With the matching event thread
// above the caller's priority (as Lumines starts it) and below. The client requests, the host accepts.
// How long the accepting SelectTarget call itself takes too. Each round uses a new context. Not a
// regression test: it prints times, and with the event thread below the caller's, ESTABLISHED has
// come either 1-10 ms or 200 ms (the retransmit interval) after the call. Built as matchingaccept_host
// and matchingaccept_client.

#include "pairmatching.h"

#define MATCHING_PORT 0x3500

#if HOST
static const char *bucket(u64 us) {
	static const int limits[] = { 1, 10, 20, 50, 100, 200 };
	static const char *names[] = { "under 1 ms", "1-10 ms", "10-20 ms", "20-50 ms", "50-100 ms", "100-200 ms" };
	for (int i = 0; i < 6; ++i) {
		if (us < limits[i] * 1000ULL)
			return names[i];
	}
	return "200 ms or more";
}
#endif

static void acceptCase(int c, const char *title, int evthPri) {
	printf("\n%s:\n", title);
	int port = MATCHING_PORT + c;
	int sync = 10 * c;
	for (int round = 0; round < 5; ++round) {
		mNumEvents = 0;
		int id = sceNetAdhocMatchingCreate(3, 2, port, 2048, 200000, 500000, 5, 200000, &matchingHandler);
		sceNetAdhocMatchingStart(id, evthPri, 0x2000, 0x30, 0x2000, 0, NULL);
#if HOST
		pairSync(sync + 1);
		// Wait for the client's request, polling often so the accept follows it closely.
		int req = -1;
		for (int i = 0; i < 50000 && req < 0; ++i) {
			for (int j = 0; j < mNumEvents; ++j) {
				if (mEvents[j].event == 2) {
					req = j;
				}
			}
			if (req < 0) {
				sceKernelDelayThread(100);
			}
		}
		if (req < 0) {
			printf("  Round %d: no REQUEST\n", round);
		} else {
			int from = mNumEvents;
			u64 t0 = sceKernelGetSystemTimeWide();
			int r = sceNetAdhocMatchingSelectTarget(id, &peerMac, 0, NULL);
			u64 t1 = sceKernelGetSystemTimeWide();
			int during = 0;
			for (int j = from; j < mNumEvents; ++j) {
				if (mEvents[j].event == 7) {
					during = 1;
				}
			}
			int est = waitMatching(7, from, 2000);
			printf("  Round %d: SelectTarget %08x in %s, ESTABLISHED before it returned: %s", round, r, bucket(t1 - t0), during ? "yes" : "no");
			if (est >= 0) {
				printf(", %s after the call\n", during ? "-" : bucket(mEvents[est].time - t0));
			} else {
				printf(", not within 2 s\n");
			}
		}
#else
		// Wait for the host's hello, then request.
		int hello = waitMatching(1, 0, 5000);
		pairSync(sync + 1);
		int r = hello >= 0 ? sceNetAdhocMatchingSelectTarget(id, &peerMac, 0, NULL) : -1;
		int est = waitMatching(7, 0, 3000);
		printf("  Round %d: SelectTarget %08x, ESTABLISHED: %s\n", round, r, est >= 0 ? "yes" : "no");
#endif
		pairSync(sync + 2);
		sceNetAdhocMatchingStop(id);
		sceNetAdhocMatchingDelete(id);
		// Don't start the next round's hellos while this one's BYE may still be on its way.
		pairSync(sync + 3);
	}
}

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	sceNetAdhocMatchingInit(0x20000);
	acceptCase(1, "Event thread above the caller's priority (0x10)", 0x10);
	acceptCase(2, "Event thread below the caller's priority (0x60)", 0x60);
	sceNetAdhocMatchingTerm();
	pairShutdown();
	return 0;
}
