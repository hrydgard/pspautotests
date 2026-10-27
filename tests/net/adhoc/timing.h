#pragma once

// Helpers for the timing measurements (timing.c, pair/timing_*.c): sample sets printed as
// min/median/max in microseconds, and an adhocctl handler that timestamps every event. These are
// measurements, not regression tests: their output varies from run to run.

#include "adhoc.h"

static inline u64 timeNow() {
	return sceKernelGetSystemTimeWide();
}

#define MAX_SAMPLES 200

typedef struct {
	const char *name;
	int count;
	u32 us[MAX_SAMPLES];
} Samples;

static inline void sampleAdd(Samples *s, u64 us) {
	if (s->count < MAX_SAMPLES) {
		s->us[s->count++] = us > 0xFFFFFFFF ? 0xFFFFFFFF : (u32)us;
	}
}

static inline void samplePrint(const char *title, Samples *s) {
	if (s->count == 0) {
		printf("  %-44s no samples\n", title);
		return;
	}
	// Insertion sort, the sets are small.
	for (int i = 1; i < s->count; ++i) {
		u32 v = s->us[i];
		int j = i - 1;
		while (j >= 0 && s->us[j] > v) {
			s->us[j + 1] = s->us[j];
			--j;
		}
		s->us[j + 1] = v;
	}
	u64 sum = 0;
	for (int i = 0; i < s->count; ++i) {
		sum += s->us[i];
	}
	printf("  %-44s n=%3d min %8u med %8u avg %8u max %8u us\n", title, s->count, (unsigned)s->us[0], (unsigned)s->us[s->count / 2], (unsigned)(sum / s->count), (unsigned)s->us[s->count - 1]);
	s->count = 0;
}

// A single measurement, printed the same way.
static inline void printOnce(const char *title, u64 us) {
	printf("  %-44s %8u us\n", title, (unsigned)us);
}

// The average cost of a call made many times.
#define TIME_CALLS(title, n, call) do { \
	u64 _start = timeNow(); \
	for (int _i = 0; _i < (n); ++_i) { call; } \
	u64 _end = timeNow(); \
	printf("  %-44s %8u.%02u us each (%d calls)\n", title, (unsigned)((_end - _start) / (n)), (unsigned)(((_end - _start) * 100 / (n)) % 100), (n)); \
} while (0)

typedef struct {
	int event;
	int error;
	u64 time;
} TimedEvent;

static volatile TimedEvent timedEvents[64];
static volatile int timedEventCount = 0;

static void timedHandler(int event, int error, void *arg) {
	if (timedEventCount < 64) {
		timedEvents[timedEventCount].event = event;
		timedEvents[timedEventCount].error = error;
		timedEvents[timedEventCount].time = timeNow();
		timedEventCount++;
	}
}

// Waits for an event arriving after index `from`, polling every 100us. Returns its index, or -1.
static inline int waitTimedEvent(int from, int timeoutMs) {
	u64 end = timeNow() + (u64)timeoutMs * 1000;
	while (timeNow() < end) {
		if (timedEventCount > from) {
			return from;
		}
		sceKernelDelayThread(100);
	}
	return -1;
}

// Polls the adhocctl state every 100us until it is `state`. Returns the time it got there, or 0.
static inline u64 waitState(int state, int timeoutMs) {
	u64 end = timeNow() + (u64)timeoutMs * 1000;
	while (timeNow() < end) {
		int s = -1;
		sceNetAdhocctlGetState(&s);
		if (s == state) {
			return timeNow();
		}
		sceKernelDelayThread(100);
	}
	return 0;
}
