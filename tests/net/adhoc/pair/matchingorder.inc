// The order of sceNetAdhocMatchingGetMembers' list on a parent and on a child, how its next pointers
// link it, and whether it changes in the seconds after the child is established. Uses a parent for
// four and a child for five, with three different hello and keepalive intervals. The child also tries
// SetHelloOpt, before and after joining. Built as matchingorder_host and matchingorder_client.

#include "pairmatching.h"

#define MATCHING_PORT 0x3500

typedef struct Member {
	struct Member *next;
	AdhocMac mac;
	u8 padding[2];
} Member;

// Prints the list in array order, then where each entry's next pointer goes ("-" for none).
static void printMembers(const char *title, int id) {
	Member members[6];
	memset(members, 0xEE, sizeof(members));
	int size = 0;
	int result = sceNetAdhocMatchingGetMembers(id, &size, NULL);
	printf("  %s: %08x, size %d", title, result, size);
	size = sizeof(members);
	result = sceNetAdhocMatchingGetMembers(id, &size, members);
	printf(", then %08x, size %d:", result, size);
	int count = size / (int)sizeof(Member);
	for (int i = 0; i < count && i < 6; ++i) {
		printf(" %s", pairMacName(&members[i].mac));
	}
	printf(" (next:");
	for (int i = 0; i < count && i < 6; ++i) {
		if (members[i].next == NULL) {
			printf(" -");
		} else if (members[i].next >= members && members[i].next < members + 6) {
			printf(" %d", (int)(members[i].next - members));
		} else {
			printf(" elsewhere");
		}
	}
	printf(")\n");
}

#if !HOST
// Whether the first member is the console itself.
static int selfFirst(int id) {
	Member members[6];
	int size = sizeof(members);
	if (sceNetAdhocMatchingGetMembers(id, &size, members) < 0 || size < (int)sizeof(Member)) {
		return -1;
	}
	return memcmp(&members[0].mac, &localMac, sizeof(AdhocMac)) == 0;
}
#endif

static void orderRound(int r, int helloUs, int keepaliveUs) {
	printf("\nRound %d, hello %d ms, keepalive %d ms:\n", r, helloUs / 1000, keepaliveUs / 1000);
	mNumEvents = 0;
#if HOST
	int id = sceNetAdhocMatchingCreate(1, 4, MATCHING_PORT + r, 1024, helloUs, keepaliveUs, 10, 500000, &matchingHandler);
#else
	int id = sceNetAdhocMatchingCreate(2, 5, MATCHING_PORT + r, 1024, helloUs, keepaliveUs, 10, 500000, &matchingHandler);
#endif
	pairSync(10 * r + 1);
#if HOST
	printf("Start: %08x\n", sceNetAdhocMatchingStart(id, 0x30, 0x2000, 0x30, 0x2000, 4, "HOST"));
	printf("Got REQUEST: %s\n", waitMatching(2, 0, 10000) >= 0 ? "yes" : "no");
	printf("SelectTarget: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 0, NULL));
	int est = waitMatching(7, 0, 5000);
	printf("Got ESTABLISHED: %s\n", est >= 0 ? "yes" : "no");
	printMembers("Members", id);
	pairSync(10 * r + 2);
	printMembers("Members later", id);
#else
	printf("Start: %08x\n", sceNetAdhocMatchingStart(id, 0x30, 0x2000, 0x30, 0x2000, 0, NULL));
	printf("SetHelloOpt before joining: %08x\n", sceNetAdhocMatchingSetHelloOpt(id, 4, "CLI1"));
	printf("Got HELLO: %s\n", waitMatching(1, 0, 10000) >= 0 ? "yes" : "no");
	printf("SelectTarget: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 4, "JOIN"));
	int est = waitMatching(7, 0, 10000);
	printf("Got ESTABLISHED: %s\n", est >= 0 ? "yes" : "no");
	u64 start = est >= 0 ? mEvents[est].time : sceKernelGetSystemTimeWide();
	printMembers("Members at once", id);
	int last = selfFirst(id);
	while (sceKernelGetSystemTimeWide() - start < 2ULL * keepaliveUs) {
		int now = selfFirst(id);
		if (now != last) {
			// In keepalive intervals, to the nearest whole one, so the output doesn't depend on network latency.
			u64 intervals = ((sceKernelGetSystemTimeWide() - start) + keepaliveUs / 2) / keepaliveUs;
			printf("  Order changed to %s first %d keepalive interval(s) after ESTABLISHED\n", now == 1 ? "self" : "parent", (int)intervals);
			last = now;
		}
		sceKernelDelayThread(20000);
	}
	printMembers("Members two keepalive intervals later", id);
	printf("SetHelloOpt after joining: %08x\n", sceNetAdhocMatchingSetHelloOpt(id, 4, "CLI2"));
	pairSync(10 * r + 2);
#endif
	printMatchingEvents(0);
	pairSync(10 * r + 3);
	sceNetAdhocMatchingStop(id);
	sceNetAdhocMatchingDelete(id);
	sceKernelDelayThread(500000);
}

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	sceNetAdhocMatchingInit(0x20000);
	orderRound(1, 1000000, 3000000);
	orderRound(2, 3000000, 1000000);
	orderRound(3, 500000, 2000000);
	sceNetAdhocMatchingTerm();
	pairShutdown();
	return 0;
}
