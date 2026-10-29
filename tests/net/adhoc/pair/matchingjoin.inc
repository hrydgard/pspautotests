// When does a host turn a client's join request into a REQUEST event? With and without the hello data
// changing (SetHelloOpt) just before, with the client joining as soon as it has the new hello data, a
// little later, or without waiting for it, and with the host changing its hello data every second. Each
// case uses a new context, so the host has never heard of the client before. Built as matchingjoin_host
// and matchingjoin_client.

#include "pairmatching.h"

#define MATCHING_PORT 0x3400

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

#if HOST
// The client can join a moment before the host's pairSync returns, so us can be a little negative.
static const char *bucket(s64 us) {
	static const int limits[] = { 1, 2, 3, 4, 6, 8, 10, 12 };
	static const char *names[] = { "under 1s", "1-2s", "2-3s", "3-4s", "4-6s", "6-8s", "8-10s", "10-12s" };
	for (int i = 0; i < 8; ++i) {
		if (us < limits[i] * 1000000LL)
			return names[i];
	}
	return "12s or more";
}
#endif

// helloOpt: SetHelloOpt just before the client joins. waitHello: the client waits for the new hello data
// before it joins. clientDelayMs: how long after that the client joins. repeatHelloOpt: SetHelloOpt every
// second while waiting. The host's times are from the client's join.
static void joinCase(int c, const char *title, int helloOpt, int waitHello, int clientDelayMs, int repeatHelloOpt) {
	printf("\n%s:\n", title);
	mNumEvents = 0;
	int port = MATCHING_PORT + c;
	int sync = 10 * c;
#if HOST
	int id = sceNetAdhocMatchingCreate(1, 2, port, 2048, 200000, 500000, 5, 200000, &matchingHandler);
	printf("  Start: %08x\n", sceNetAdhocMatchingStart(id, 0x30, 0x2000, 0x30, 0x2000, 4, "OPT1"));
	pairSync(sync + 1);
	if (helloOpt) {
		printf("  SetHelloOpt: %08x\n", sceNetAdhocMatchingSetHelloOpt(id, 4, "OPT2"));
	}
	pairSync(sync + 2);
	u64 start = sceKernelGetSystemTimeWide();
	int got = -1;
	for (int i = 0; i < 15 && got < 0; ++i) {
		if (repeatHelloOpt) {
			sceNetAdhocMatchingSetHelloOpt(id, 4, (i & 1) ? "OPT2" : "OPT3");
		}
		got = waitMatching(2, 0, 1000);
	}
	if (got >= 0) {
		printf("  Got REQUEST: %s\n", bucket((s64)(mEvents[got].time - start) - clientDelayMs * 1000LL));
		printf("  CancelTarget: %08x\n", sceNetAdhocMatchingCancelTarget(id, &peerMac));
	} else {
		printf("  Got REQUEST: no, in 15s\n");
	}
	pairSync(sync + 3);
	// Up to the REQUEST: what comes after the cancel depends on timing.
	printMatchingEventsUntil(0, got >= 0 ? got + 1 : mNumEvents);
#else
	int id = sceNetAdhocMatchingCreate(2, 2, port, 2048, 200000, 500000, 5, 200000, &matchingHandler);
	sceNetAdhocMatchingStart(id, 0x30, 0x2000, 0x30, 0x2000, 0, NULL);
	printf("  Got HELLO 'OPT1': %s\n", waitHelloWith("OPT1", 0, 5000) >= 0 ? "yes" : "no");
	pairSync(sync + 1);
	if (helloOpt && waitHello) {
		printf("  Got HELLO 'OPT2': %s\n", waitHelloWith("OPT2", 0, 5000) >= 0 ? "yes" : "no");
	}
	pairSync(sync + 2);
	if (clientDelayMs) {
		sceKernelDelayThread(clientDelayMs * 1000);
	}
	int from = mNumEvents;
	printf("  SelectTarget: %08x\n", sceNetAdhocMatchingSelectTarget(id, &peerMac, 4, "JOIN"));
	// The host cancels once it has the request, or gives up after 15 seconds.
	int deny = waitMatching(4, from, 17000);
	printf("  Got DENY: %s\n", deny >= 0 ? "yes" : "no");
	pairSync(sync + 3);
	printMatchingEventsUntil(from, deny >= 0 ? deny + 1 : mNumEvents);
#endif
	sceNetAdhocMatchingStop(id);
	sceNetAdhocMatchingDelete(id);
	// Don't start the next case's hellos while this one's BYE may still be on its way.
	pairSync(sync + 4);
}

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	sceNetAdhocMatchingInit(0x20000);
	joinCase(1, "Join, hello data unchanged", 0, 0, 0, 0);
	joinCase(2, "Join as soon as the new hello data arrives", 1, 1, 0, 0);
	joinCase(3, "Join 0.5 seconds after the new hello data", 1, 1, 500, 0);
	joinCase(4, "Join 1 second after the new hello data", 1, 1, 1000, 0);
	joinCase(5, "Join 2 seconds after the new hello data", 1, 1, 2000, 0);
	joinCase(6, "Join right after SetHelloOpt, without waiting for the new hello data", 1, 0, 0, 0);
	joinCase(7, "Join while the host changes its hello data every second", 0, 0, 0, 1);
	sceNetAdhocMatchingTerm();
	pairShutdown();
	return 0;
}
