// Whether a receiver polling fast sees two back-to-back sends as two reads. The client sends pairs of
// messages with no delay between them; the host polls PtpRecv without blocking every 100 us and records
// the lengths it reads and the time between the reads. pair/ptpmsg shows that sends that have both
// arrived come back together. Capcom Classics Collection Reloaded relies on the split. Built as
// ptpsegments_host and ptpsegments_client.

#include "pair.h"
#include "../timing.h"

#define PTP_PORT 0x5A00
#define REPS 8

static const int firsts[] = { 6, 4, 100, 1000 };
static const int seconds_[] = { 55, 4, 100, 1000 };
#define PAIRS 4

static u8 buf[4096];

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	int step = 10;
#if HOST
	int listener = sceNetAdhocPtpListen(&localMac, PTP_PORT, 0x2000, 200000, 5, 1, 0);
	pairSync(step++);
	int c = sceNetAdhocPtpAccept(listener, NULL, NULL, 3000000, 0);
	printf("Accept: %s\n", c > 0 ? "ok" : "failed");
	pairSync(step++);
	for (int p = 0; p < PAIRS; ++p) {
		int split = 0, merged = 0, other = 0;
		Samples gap = { 0 };
		for (int r = 0; r < REPS; ++r) {
			pairSync(step++);
			int want = firsts[p] + seconds_[p];
			int got = 0, reads = 0, lens[8];
			u64 times[8];
			u64 t0 = timeNow();
			while (got < want && timeNow() - t0 < 2000000) {
				int len = sizeof(buf);
				if (sceNetAdhocPtpRecv(c, buf, &len, 0, 1) == 0 && len > 0) {
					if (reads < 8) {
						lens[reads] = len;
						times[reads] = timeNow();
					}
					reads++;
					got += len;
				}
				sceKernelDelayThread(100);
			}
			if (reads == 1 && lens[0] == want) {
				merged++;
			} else if (reads == 2 && lens[0] == firsts[p] && lens[1] == seconds_[p]) {
				split++;
				sampleAdd(&gap, times[1] - times[0]);
			} else {
				other++;
			}
			pairSync(step++);
		}
		printf("%4d + %4d: as sent %d/%d, together %d/%d, otherwise %d/%d\n", firsts[p], seconds_[p], split, REPS, merged, REPS, other, REPS);
		char title[64];
		snprintf(title, sizeof(title), "%4d + %4d: between the two reads", firsts[p], seconds_[p]);
		samplePrint(title, &gap);
	}
	sceNetAdhocPtpClose(c, 0);
	sceNetAdhocPtpClose(listener, 0);
#else
	pairSync(step++);
	int c = sceNetAdhocPtpOpen(&localMac, 0, &peerMac, PTP_PORT, 0x2000, 200000, 5, 0);
	printf("Connect: %08x\n", sceNetAdhocPtpConnect(c, 3000000, 0));
	pairSync(step++);
	memset(buf, 7, sizeof(buf));
	for (int p = 0; p < PAIRS; ++p) {
		for (int r = 0; r < REPS; ++r) {
			pairSync(step++);
			// Make sure the host is polling by now.
			sceKernelDelayThread(20000);
			int len = firsts[p];
			sceNetAdhocPtpSend(c, buf, &len, 2000000, 0);
			len = seconds_[p];
			sceNetAdhocPtpSend(c, buf, &len, 2000000, 0);
			pairSync(step++);
		}
	}
	sceNetAdhocPtpClose(c, 0);
#endif
	pairShutdown();
	return 0;
}
