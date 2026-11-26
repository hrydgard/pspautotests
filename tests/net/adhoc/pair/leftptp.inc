// Whether reading a console's PTP data after it has left the group still works, and whether it brings
// the console back into the peer list (pair/leftdata asks the same of PDP). The client connects, sends
// four messages, flushes and leaves at once; the host leaves them unread until the client is gone from
// its peer list, reads them, and then watches the peer list for two seconds.
// Built as leftptp_host and leftptp_client.

#include "pair.h"

#define PTP_PORT 0x7712

#if HOST
static int peerCount() {
	AdhocctlPeerInfo peers[4];
	int size = sizeof(peers);
	if (sceNetAdhocctlGetPeerList(&size, peers) < 0) {
		return -1;
	}
	return size / (int)sizeof(AdhocctlPeerInfo);
}

// Polls the peer list every millisecond for up to timeoutMs, until it has `want` peers.
static int waitPeerCount(int want, int timeoutMs) {
	for (int i = 0; i < timeoutMs; ++i) {
		if (peerCount() == want) {
			return 1;
		}
		sceKernelDelayThread(1000);
	}
	return 0;
}
#endif

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
#if HOST
	int listener = sceNetAdhocPtpListen(&localMac, PTP_PORT, 0x2000, 200000, 5, 1, 0);
	pairSync(10);
	int ptp = sceNetAdhocPtpAccept(listener, NULL, NULL, 5000000, 0);
	printf("Accept: %s\n", ptp > 0 ? "ok" : "failed");
	pairSync(11);
	printf("Client gone from the peer list: %s\n", waitPeerCount(0, 5000) ? "yes" : "no");
	sceKernelDelayThread(100000);
	u8 buf[64];
	int len = sizeof(buf);
	int result = sceNetAdhocPtpRecv(ptp, buf, &len, 0, 1);
	printf("Recv after that: %08x, len %d, %s\n", result, result == 0 ? len : 0, result == 0 && len >= 4 && memcmp(buf, "LEFT", 4) == 0 ? "the client's data" : "nothing");
	printf("Client back in the peer list within 2 s: %s\n", waitPeerCount(1, 2000) ? "yes" : "no");
	AdhocctlPeerInfo info;
	printf("GetPeerInfo for it: %08x\n", sceNetAdhocctlGetPeerInfo(&peerMac, sizeof(info), &info));
	len = sizeof(buf);
	printf("Recv again: %08x\n", sceNetAdhocPtpRecv(ptp, buf, &len, 0, 1));
	sceNetAdhocPtpClose(ptp, 0);
	sceNetAdhocPtpClose(listener, 0);
#else
	pairSync(10);
	int ptp = sceNetAdhocPtpOpen(&localMac, 0, &peerMac, PTP_PORT, 0x2000, 200000, 5, 0);
	printf("Connect: %08x\n", sceNetAdhocPtpConnect(ptp, 5000000, 0));
	pairSync(11);
	int failed = 0;
	for (int i = 0; i < 4; ++i) {
		int len = 4;
		if (sceNetAdhocPtpSend(ptp, "LEFT", &len, 1000000, 0) != 0) {
			failed++;
		}
	}
	printf("Sends failed: %d, flush %08x\n", failed, sceNetAdhocPtpFlush(ptp, 1000000, 0));
	adhocctlLeaveGroup();
	// Stay away while the host watches.
	sceKernelDelayThread(6000000);
	sceNetAdhocPtpClose(ptp, 0);
#endif
	pairShutdown();
	return 0;
}
