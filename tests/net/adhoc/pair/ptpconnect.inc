// What a non-blocking PTP connect looks like while it's in progress: the client opens a connection,
// calls PtpConnect without blocking and then every 200 us reads its state from GetPtpStat and polls
// it for CONNECT and SEND, until it's established. The host polls its listener for ACCEPT meanwhile.
// Five rounds. Not a regression test: it prints times. Built as ptpconnect_host and ptpconnect_client.

#include "pair.h"
#include "../timing.h"

#define PTP_PORT 0x5700
#define ROUNDS 5

static int pollEvents(int id, int events) {
	AdhocPollSd sd;
	sd.id = id;
	sd.events = events;
	sd.revents = 0;
	sceNetAdhocPollSocket(&sd, 1, 0, 1);
	return sd.revents;
}

#if !HOST
static int ptpState(int id) {
	static PtpStat stats[8];
	int size = sizeof(stats);
	if (sceNetAdhocGetPtpStat(&size, stats) < 0) {
		return -1;
	}
	for (int i = 0; i < size / (int)sizeof(PtpStat) && i < 8; ++i) {
		if (stats[i].id == id) {
			return stats[i].state;
		}
	}
	return -2;
}
#endif

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
#if HOST
	int listener = sceNetAdhocPtpListen(&localMac, PTP_PORT, 0x2000, 200000, 5, ROUNDS, 0);
	printf("Listen: %s\n", listener > 0 ? "ok" : "failed");
#endif
	for (int r = 1; r <= ROUNDS; ++r) {
		pairSync(10 * r);
#if HOST
		u64 start = timeNow();
		// Watch the listener until a connection can be accepted.
		u64 acceptReady = 0;
		while (timeNow() - start < 2000000) {
			if (pollEvents(listener, 8) & 8) {
				acceptReady = timeNow() - start;
				break;
			}
			sceKernelDelayThread(200);
		}
		int ptp = sceNetAdhocPtpAccept(listener, NULL, NULL, 2000000, 0);
		printf("Round %d: listener ACCEPT after %u us, Accept %s\n", r, (unsigned)acceptReady, ptp > 0 ? "ok" : "failed");
		pairSync(10 * r + 1);
		if (ptp > 0) {
			sceNetAdhocPtpClose(ptp, 0);
		}
#else
		int ptp = sceNetAdhocPtpOpen(&localMac, 0, &peerMac, PTP_PORT, 0x2000, 200000, 5, 0);
		int openState = ptpState(ptp);
		u64 t0 = timeNow();
		int result = sceNetAdhocPtpConnect(ptp, 0, 1);
		printf("Round %d: state after Open %d, non-blocking Connect %08x\n", r, openState, result);
		int last = -100;
		u64 connectAt = 0, sendAt = 0;
		printf("  States:");
		while (timeNow() - t0 < 300000) {
			u64 t = timeNow() - t0;
			int state = ptpState(ptp);
			if (state != last) {
				printf(" [%u us: %d]", (unsigned)t, state);
				last = state;
			}
			int revents = pollEvents(ptp, 4 | 1);
			if (!connectAt && (revents & 4)) {
				connectAt = t ? t : 1;
			}
			if (!sendAt && (revents & 1)) {
				sendAt = t ? t : 1;
			}
			if (state == 4 && connectAt && sendAt) {
				break;
			}
			sceKernelDelayThread(200);
		}
		printf("\n  Poll CONNECT at %u us, SEND at %u us\n", (unsigned)connectAt, (unsigned)sendAt);
		printf("  Connect again: %08x\n", sceNetAdhocPtpConnect(ptp, 0, 1));
		pairSync(10 * r + 1);
		sceNetAdhocPtpClose(ptp, 0);
#endif
	}
#if HOST
	sceNetAdhocPtpClose(listener, 0);
#endif
	pairShutdown();
	return 0;
}
