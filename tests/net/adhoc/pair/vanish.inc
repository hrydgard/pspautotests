// What a console sees when its peer drops off the network without closing anything: the client
// leaves the ad hoc network (sceNetAdhocctlDisconnect) with a PTP connection and a matching session
// still open. Built as vanish_host and vanish_client.

#include "pairmatching.h"

#define PTP_PORT 0x2100
#define MATCHING_PORT 0x3100
#define PDP_PORT 0x1100

static u8 buffer[4096];

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}

	printf("\nSetup:\n");
	sceNetAdhocMatchingInit(0x20000);
	int pdp = sceNetAdhocPdpCreate(&localMac, PDP_PORT, 0x2000, 0);
#if HOST
	int listener = sceNetAdhocPtpListen(&localMac, PTP_PORT, 0x8000, 200000, 5, 1, 0);
	// Keep-alive every 500ms, 5 tries: on the PSP a peer times out after about 2.5s of silence.
	int matching = sceNetAdhocMatchingCreate(1, 4, MATCHING_PORT, 2048, 200000, 500000, 5, 200000, &matchingHandler);
	sceNetAdhocMatchingStart(matching, 0x30, 0x2000, 0x30, 0x2000, 0, NULL);
	pairSync(1);
	int ptp = sceNetAdhocPtpAccept(listener, NULL, NULL, 5000000, 0);
	printf("  PTP: %s\n", ptp > 0 ? "accepted" : "failed");
	int req = waitMatching(2, 0, 10000);
	if (req >= 0) {
		sceNetAdhocMatchingSelectTarget(matching, &peerMac, 0, NULL);
	}
	printf("  Matching: %s\n", waitMatching(7, 0, 5000) >= 0 ? "established" : "failed");
#else
	int matching = sceNetAdhocMatchingCreate(2, 2, MATCHING_PORT, 2048, 200000, 500000, 5, 200000, &matchingHandler);
	sceNetAdhocMatchingStart(matching, 0x30, 0x2000, 0x30, 0x2000, 0, NULL);
	pairSync(1);
	int ptp = sceNetAdhocPtpOpen(&localMac, PTP_PORT, &peerMac, PTP_PORT, 0x8000, 200000, 5, 0);
	printf("  PTP: %s\n", sceNetAdhocPtpConnect(ptp, 5000000, 0) == 0 ? "connected" : "failed");
	if (waitMatching(1, 0, 10000) >= 0) {
		sceNetAdhocMatchingSelectTarget(matching, &peerMac, 0, NULL);
	}
	printf("  Matching: %s\n", waitMatching(7, 0, 10000) >= 0 ? "established" : "failed");
#endif
	pairSync(2);
	int start = mNumEvents;

#if HOST
	printf("\nThe client vanishes:\n");
	// From here on, nothing more from the client. Watch for up to 40 seconds.
	u64 begin = sceKernelGetSystemTimeWide();
	u64 ptpSendFailed = 0, ptpRecvFailed = 0, peerGone = 0, matchingTimeout = 0;
	int ptpSendError = 0, ptpRecvError = 0;
	int pdpSends = 0, pdpSendFailures = 0;
	while (sceKernelGetSystemTimeWide() - begin < 40000000) {
		u64 now = sceKernelGetSystemTimeWide() - begin;
		if (!ptpSendFailed) {
			int len = 100;
			int result = sceNetAdhocPtpSend(ptp, buffer, &len, 0, 1);
			if (result < 0 && result != (int)0x80410709) {
				ptpSendFailed = now;
				ptpSendError = result;
			}
		}
		if (!ptpRecvFailed) {
			int len = sizeof(buffer);
			int result = sceNetAdhocPtpRecv(ptp, buffer, &len, 0, 1);
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
		int timeoutEvent = matchingTimeout ? -1 : waitMatching(8, start, 0);
		if (timeoutEvent >= 0) {
			matchingTimeout = mEvents[timeoutEvent].time - begin;
		}
		pdpSends++;
		if (sceNetAdhocPdpSend(pdp, &peerMac, PDP_PORT, buffer, 16, 0, 1) < 0) {
			pdpSendFailures++;
		}
		if (ptpSendFailed && ptpRecvFailed && peerGone && matchingTimeout) {
			break;
		}
		sceKernelDelayThread(100000);
	}
	printf("  PTP send fails: %s, %08x\n", ptpSendFailed ? timeBucket(ptpSendFailed) : "not within 40s", ptpSendError);
	printf("  PTP recv fails: %s, %08x\n", ptpRecvFailed ? timeBucket(ptpRecvFailed) : "not within 40s", ptpRecvError);
	printf("  Peer gone from the peer list: %s\n", peerGone ? timeBucket(peerGone) : "not within 40s");
	printf("  Matching TIMEOUT: %s\n", matchingTimeout ? timeBucket(matchingTimeout) : "not within 40s");
	printf("  PDP sends to the peer: %s\n", pdpSendFailures == 0 ? "all succeed" : (pdpSendFailures == pdpSends ? "all fail" : "some fail"));
	printMatchingEvents(start);
	int members = 0;
	sceNetAdhocMatchingGetMembers(matching, &members, NULL);
	printf("  Matching members: %d\n", members / 12);
	int state = -1;
	sceNetAdhocctlGetState(&state);
	printf("  adhocctl state: %d, events: %d\n", state, eventCount);
#else
	printf("\nVanishing:\n");
	int disconnected = sceNetAdhocctlDisconnect();
	printf("  sceNetAdhocctlDisconnect: %08x\n", disconnected);
	sceKernelDelayThread(3000000);
	int len = 100;
	printf("  PTP send afterwards: %08x\n", sceNetAdhocPtpSend(ptp, buffer, &len, 0, 1));
	len = sizeof(buffer);
	// Whether some of the host's data is still waiting varies, so 0 and WOULD_BLOCK count the same.
	int recvResult = sceNetAdhocPtpRecv(ptp, buffer, &len, 0, 1);
	printf("  PTP recv afterwards: %s\n", recvResult == 0 || recvResult == (int)0x80410709 ? "no error" : "error");
	printf("  PDP send afterwards: %08x\n", sceNetAdhocPdpSend(pdp, &peerMac, PDP_PORT, buffer, 16, 0, 1));
	printMatchingEvents(start);
	// Stay quiet while the host watches.
	sceKernelDelayThread(37000000);
#endif

	sceNetAdhocMatchingStop(matching);
	sceNetAdhocMatchingDelete(matching);
	sceNetAdhocMatchingTerm();
	sceNetAdhocPtpClose(ptp, 0);
#if HOST
	sceNetAdhocPtpClose(listener, 0);
#endif
	sceNetAdhocPdpDelete(pdp, 0);
	pairShutdown();
	return 0;
}
