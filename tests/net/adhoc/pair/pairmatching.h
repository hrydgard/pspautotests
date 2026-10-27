#pragma once

// Matching helpers for the pair tests: a callback that records events in order (with runs of
// HELLOs folded into one), waiting for an event, and printing what arrived.

#include "pair.h"

typedef struct {
	int id;
	int event;
	int fromPeer;
	int optlen;
	char opt[16];
	u64 time;
} MatchingEvent;

static volatile MatchingEvent mEvents[48];
static volatile int mNumEvents = 0;

static inline void matchingHandler(int id, int event, AdhocMac *mac, int optlen, void *opt) {
	if (event == 1 && mNumEvents > 0 && mEvents[mNumEvents - 1].event == 1 && mEvents[mNumEvents - 1].id == id) {
		// Same opt as the last hello? Then it's the same advertisement again.
		if (optlen == mEvents[mNumEvents - 1].optlen && (optlen == 0 || memcmp(opt, (const void *)mEvents[mNumEvents - 1].opt, optlen < 15 ? optlen : 15) == 0)) {
			return;
		}
	}
	if (mNumEvents >= 48) {
		return;
	}
	volatile MatchingEvent *e = &mEvents[mNumEvents];
	e->id = id;
	e->event = event;
	e->fromPeer = mac != NULL && memcmp(mac, &peerMac, sizeof(AdhocMac)) == 0;
	e->optlen = optlen;
	e->time = sceKernelGetSystemTimeWide();
	memset((void *)e->opt, 0, sizeof(e->opt));
	if (opt != NULL && optlen > 0) {
		memcpy((void *)e->opt, opt, optlen < 15 ? optlen : 15);
	}
	mNumEvents++;
}

// Waits up to timeoutMs for an event of the given type, arriving after index `from`. Returns its
// index, or -1.
static inline int waitMatching(int event, int from, int timeoutMs) {
	for (int i = 0; i <= timeoutMs / 10; ++i) {
		for (int j = from; j < mNumEvents; ++j) {
			if (mEvents[j].event == event) {
				return j;
			}
		}
		sceKernelDelayThread(10000);
	}
	return -1;
}

static inline void printMatchingEvents(int from) {
	printf("  Events:");
	for (int i = from; i < mNumEvents; ++i) {
		printf(" %d", mEvents[i].event);
		if (mEvents[i].optlen > 0) {
			printf("('%s')", mEvents[i].opt);
		}
		if (!mEvents[i].fromPeer) {
			printf("(not from peer)");
		}
	}
	printf("\n");
}

// How long something took, in coarse buckets, so that the output doesn't depend on exact timing.
static inline const char *timeBucket(u64 us) {
	if (us < 1000000)
		return "under 1s";
	if (us < 2000000)
		return "1-2s";
	if (us < 4000000)
		return "2-4s";
	if (us < 8000000)
		return "4-8s";
	if (us < 16000000)
		return "8-16s";
	if (us < 32000000)
		return "16-32s";
	return "over 32s";
}
