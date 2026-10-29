// When a console notices that its peer has dropped off the network, with several retransmit and
// keepalive settings. In each round the client sends a datagram and then leaves the network at once
// (sceNetAdhocctlDisconnect) with a PTP connection and a matching session open; the host times
// everything from that datagram's arrival, and the client joins again for the next round. The rounds
// vary the intervals and the counts separately, to tell the formulas apart. Not a regression test: it
// prints times. Built as vanishtiming_host and vanishtiming_client.

#include "pairmatching.h"

#define PTP_PORT 0x2200
#define MATCHING_PORT 0x3600
#define GO_PORT 0x1200

typedef struct {
	int ptpRexmtMs, ptpRexmtCount;
	int keepaliveMs, matchingRexmtMs, initCount;
	// How long the client waits after the sync before it leaves.
	int leaveDelayMs;
} Settings;

#if HOST
static u8 buffer[4096];

static void printTime(const char *what, u64 us) {
	if (us) {
		printf("  %-34s %5u ms\n", what, (unsigned)(us / 1000));
	} else {
		printf("  %-34s not within 9s\n", what);
	}
}
#endif

static void vanishRound(int r, const Settings *s) {
	printf("\nRound %d: PTP rexmt %d ms x %d, matching keepalive %d ms, rexmt %d ms, init count %d, leaving %d ms after the sync:\n",
		r, s->ptpRexmtMs, s->ptpRexmtCount, s->keepaliveMs, s->matchingRexmtMs, s->initCount, s->leaveDelayMs);
	mNumEvents = 0;
	int sync = 10 * r;
	int go = sceNetAdhocPdpCreate(&localMac, GO_PORT, 0x1000, 0);
#if HOST
	int listener = sceNetAdhocPtpListen(&localMac, PTP_PORT + r, 0x8000, s->ptpRexmtMs * 1000, s->ptpRexmtCount, 1, 0);
	int matching = sceNetAdhocMatchingCreate(1, 4, MATCHING_PORT + r, 2048, 200000, s->keepaliveMs * 1000, s->initCount, s->matchingRexmtMs * 1000, &matchingHandler);
	printf("  MatchingCreate: %s\n", matching > 0 ? "ok" : "failed");
	if (matching > 0) {
		sceNetAdhocMatchingStart(matching, 0x30, 0x2000, 0x30, 0x2000, 0, NULL);
	}
	pairSync(sync + 1);
	int ptp = sceNetAdhocPtpAccept(listener, NULL, NULL, 5000000, 0);
	u64 acceptedAt = sceKernelGetSystemTimeWide();
	printf("  PTP: %s\n", ptp > 0 ? "accepted" : "failed");
	if (matching > 0) {
		if (waitMatching(2, 0, 10000) >= 0) {
			sceNetAdhocMatchingSelectTarget(matching, &peerMac, 0, NULL);
		}
		printf("  Matching: %s\n", waitMatching(7, 0, 5000) >= 0 ? "established" : "failed");
	}
	pairSync(sync + 2);

	// Time zero: the client's last datagram, sent just before it leaves.
	AdhocMac mac;
	u16 port;
	int len = sizeof(buffer);
	if (sceNetAdhocPdpRecv(go, &mac, &port, buffer, &len, 5000000, 0) < 0) {
		printf("  No GO from the client\n");
	}
	u64 begin = sceKernelGetSystemTimeWide();
	int start = mNumEvents;
	u64 ptpSendFailed = 0, ptpRecvFailed = 0, peerGone = 0, matchingTimeout = 0, dataTimeout = 0;
	int ptpSendError = 0, ptpRecvError = 0, sendData = -1;
	u64 sendDataAt = 0, lastPtpSend = 0;
	int sendStart = -1;
	while (sceKernelGetSystemTimeWide() - begin < 9000000) {
		u64 now = sceKernelGetSystemTimeWide() - begin;
		// Data to the client keeps the PTP connection waiting for acknowledgements.
		if (!ptpSendFailed && now - lastPtpSend >= 10000) {
			lastPtpSend = now;
			int sendLen = 100;
			int result = sceNetAdhocPtpSend(ptp, buffer, &sendLen, 0, 1);
			if (result < 0 && result != (int)0x80410709) {
				ptpSendFailed = now;
				ptpSendError = result;
			}
		}
		if (!ptpRecvFailed) {
			int recvLen = sizeof(buffer);
			int result = sceNetAdhocPtpRecv(ptp, buffer, &recvLen, 0, 1);
			if (result < 0 && result != (int)0x80410709) {
				ptpRecvFailed = now;
				ptpRecvError = result;
			}
		}
		if (!peerGone) {
			int size = 0;
			sceNetAdhocctlGetPeerList(&size, NULL);
			if (size == 0) {
				peerGone = now;
			}
		}
		if (matching > 0) {
			// Matching data the client won't acknowledge, sent 300 ms in so that it can't arrive in time.
			if (!sendDataAt && now >= 300000) {
				sendStart = mNumEvents;
				sendData = sceNetAdhocMatchingSendData(matching, &peerMac, 4, "LOST");
				sendDataAt = sceKernelGetSystemTimeWide();
			}
			int e = matchingTimeout ? -1 : waitMatching(8, start, 0);
			if (e >= 0) {
				matchingTimeout = mEvents[e].time - begin;
			}
			e = dataTimeout || sendStart < 0 ? -1 : waitMatching(13, sendStart, 0);
			if (e >= 0) {
				dataTimeout = mEvents[e].time - sendDataAt;
			}
		}
		if (ptpSendFailed && ptpRecvFailed && peerGone && (matching <= 0 || (matchingTimeout && dataTimeout))) {
			break;
		}
		sceKernelDelayThread(1000);
	}
	printTime("Peer gone from the peer list:", peerGone);
	printTime("PTP send fails:", ptpSendFailed);
	printTime("PTP recv fails:", ptpRecvFailed);
	printf("  PTP errors: %08x %08x\n", ptpSendError, ptpRecvError);
	// Theory: the PSP checks the age of the oldest unacknowledged data every rexmt_int, counting from
	// when the connection was established, and gives up at the first check where it's older than
	// (rexmt_cnt + 1) * rexmt_int. That data was sent right after time zero.
	u64 interval = s->ptpRexmtMs * 1000ULL;
	u64 limit = interval * (s->ptpRexmtCount + 1);
	u64 tick = acceptedAt;
	while (tick <= begin + limit) {
		tick += interval;
	}
	printTime("PTP fails, predicted:", tick - begin);
	if (matching > 0) {
		printTime("Matching TIMEOUT:", matchingTimeout);
		printf("  SendData at 300 ms: %08x\n", sendData);
		printTime("DATA_TIMEOUT after SendData:", dataTimeout);
		printMatchingEvents(start);
		sceNetAdhocMatchingStop(matching);
		sceNetAdhocMatchingDelete(matching);
	}
	sceNetAdhocPtpClose(ptp, 0);
	sceNetAdhocPtpClose(listener, 0);
#else
	int matching = sceNetAdhocMatchingCreate(2, 2, MATCHING_PORT + r, 2048, 200000, s->keepaliveMs * 1000, s->initCount, s->matchingRexmtMs * 1000, &matchingHandler);
	if (matching > 0) {
		sceNetAdhocMatchingStart(matching, 0x30, 0x2000, 0x30, 0x2000, 0, NULL);
	}
	pairSync(sync + 1);
	int ptp = sceNetAdhocPtpOpen(&localMac, PTP_PORT + r, &peerMac, PTP_PORT + r, 0x8000, s->ptpRexmtMs * 1000, s->ptpRexmtCount, 0);
	printf("  PTP: %s\n", sceNetAdhocPtpConnect(ptp, 5000000, 0) == 0 ? "connected" : "failed");
	if (matching > 0) {
		if (waitMatching(1, 0, 10000) >= 0) {
			sceNetAdhocMatchingSelectTarget(matching, &peerMac, 0, NULL);
		}
		printf("  Matching: %s\n", waitMatching(7, 0, 10000) >= 0 ? "established" : "failed");
	}
	pairSync(sync + 2);
	sceKernelDelayThread(s->leaveDelayMs * 1000);
	sceNetAdhocPdpSend(go, &peerMac, GO_PORT, "GO", 2, 0, 1);
	sceNetAdhocctlDisconnect();
	// Stay away while the host watches.
	sceKernelDelayThread(9500000);
	if (matching > 0) {
		sceNetAdhocMatchingStop(matching);
		sceNetAdhocMatchingDelete(matching);
	}
	sceNetAdhocPtpClose(ptp, 0);
	// Back for the next round.
	if (!pairJoin()) {
		printf("  Couldn't join again\n");
	}
	pairFindPeer();
#endif
	sceNetAdhocPdpDelete(go, 0);
	// The client takes a while to join again.
	for (int i = 0; i < 3 && !pairSync(sync + 3); ++i) {
		continue;
	}
}

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	sceNetAdhocMatchingInit(0x20000);
	printf("\nKeepalive 0:\n");
	for (int mode = 1; mode <= 3; ++mode) {
		int id = sceNetAdhocMatchingCreate(mode, 2, MATCHING_PORT + 0x10 + mode, 2048, 200000, 0, 5, 200000, &matchingHandler);
		printf("  MatchingCreate, mode %d: %08x\n", mode, id > 0 ? 0 : id);
		if (id > 0) {
			sceNetAdhocMatchingDelete(id);
		}
	}
	// PTP: rexmt interval, count. Matching: keepalive interval, rexmt interval, init count.
	static const Settings settings[] = {
		{ 200, 5, 500, 200, 5, 200 },
		{ 500, 2, 250, 100, 10, 200 },
		{ 100, 10, 1250, 500, 2, 200 },
		{ 200, 2, 500, 200, 2, 200 },
		{ 500, 5, 1000, 400, 5, 200 },
		{ 100, 3, 200, 100, 5, 200 },
		{ 500, 2, 1000, 200, 2, 500 },
		{ 500, 2, 1000, 200, 2, 800 },
		{ 500, 2, 1000, 200, 2, 200 },
	};
	for (int r = 0; r < (int)(sizeof(settings) / sizeof(settings[0])); ++r) {
		vanishRound(r + 1, &settings[r]);
	}
	sceNetAdhocMatchingTerm();
	pairShutdown();
	return 0;
}
