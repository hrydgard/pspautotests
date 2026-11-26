// Whether a PTP connection gets through before the other console is in the peer list (pair/joindata
// asks the same of PDP). The host creates a group and listens at once; the client scans for the group,
// joins, and at the CONNECT event opens a connection and connects without blocking. Each side notes
// when the other showed up in its peer list and when the connection was there. Then the client sends
// four bytes. Both start in the pair group. Built as joinptp_host and joinptp_client.

// Define SHOW_TIMES to also print the times, which differ from run to run.
#include "pair.h"

#define PTP_GROUP "JOINPTP"
#define PTP_PORT 0x7711

static int peerCount() {
	AdhocctlPeerInfo peers[4];
	int size = sizeof(peers);
	if (sceNetAdhocctlGetPeerList(&size, peers) < 0) {
		return -1;
	}
	return size / (int)sizeof(AdhocctlPeerInfo);
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
	pairSync(10);
	adhocctlLeaveGroup();
	s64 listed = -1, connected = -1, data = -1;
	int connectedFirst = 0;
#if HOST
	sceKernelDelayThread(500000);
	int start = eventCount;
	sceNetAdhocctlCreate(PTP_GROUP);
	waitEvent(start, 5000);
	int listener = sceNetAdhocPtpListen(&localMac, PTP_PORT, 0x2000, 200000, 5, 1, 0);
	printf("Listen: %s\n", listener > 0 ? "ok" : "failed");
	int ptp = -1;
	u64 begin = sceKernelGetSystemTimeWide();
	u64 doneAt = 0;
	for (;;) {
		u64 now = sceKernelGetSystemTimeWide();
		if (now - begin > 12000000 || (doneAt != 0 && now > doneAt)) {
			break;
		}
		if (listed < 0 && peerCount() > 0) {
			listed = (s64)(now - begin);
		}
		if (ptp <= 0) {
			AdhocMac mac;
			u16 port = 0;
			ptp = sceNetAdhocPtpAccept(listener, &mac, &port, 0, 1);
			if (ptp > 0) {
				connected = (s64)(now - begin);
				connectedFirst = listed < 0;
			}
		} else if (data < 0) {
			u8 buf[16];
			int len = sizeof(buf);
			if (sceNetAdhocPtpRecv(ptp, buf, &len, 0, 1) == 0 && len > 0) {
				data = (s64)(now - begin);
			}
		}
		if (doneAt == 0 && listed >= 0 && data >= 0) {
			doneAt = now + 300000;
		}
		sceKernelDelayThread(1000);
	}
	printf("Client in the peer list: %s\n", listed >= 0 ? "yes" : "never");
	printf("Accepted a connection: %s\n", connected < 0 ? "never" : (connectedFirst ? "yes, before the client was in the peer list" : "yes, after the client was in the peer list"));
	printf("Data on it: %s\n", data >= 0 ? "yes" : "none");
#ifdef SHOW_TIMES
	printf("[times] listed %lld us, accepted %lld us, data %lld us\n", listed, connected, data);
#endif
	if (ptp > 0) {
		sceNetAdhocPtpClose(ptp, 0);
	}
	sceNetAdhocPtpClose(listener, 0);
#else
	sceKernelDelayThread(2000000);
	AdhocctlScanInfo info;
	int found = 0;
	for (int scans = 0; scans < 5 && !found; ++scans) {
		int start = eventCount;
		if (sceNetAdhocctlScan() < 0 || waitEvent(start, 3000) != 3) {
			sceKernelDelayThread(200000);
			continue;
		}
		AdhocctlScanInfo infos[8];
		int size = sizeof(infos);
		if (sceNetAdhocctlGetScanInfo(&size, infos) >= 0) {
			for (int i = 0; i < size / (int)sizeof(AdhocctlScanInfo) && i < 8; ++i) {
				if (memcmp(infos[i].group, PTP_GROUP, 8) == 0) {
					info = infos[i];
					found = 1;
				}
			}
		}
	}
	if (!found) {
		printf("Group not found\n");
	} else {
		int start = eventCount;
		u64 begin = sceKernelGetSystemTimeWide();
		sceNetAdhocctlJoin(&info);
		int event = waitEvent(start, 5000);
		s64 joined = (s64)(sceKernelGetSystemTimeWide() - begin);
		printf("Join: event %d\n", event);
		int ptp = sceNetAdhocPtpOpen(&localMac, 0, &peerMac, PTP_PORT, 0x2000, 200000, 5, 0);
		int result = sceNetAdhocPtpConnect(ptp, 0, 1);
		printf("Open: %s, non-blocking Connect %08x, host in the peer list by then: %s\n", ptp > 0 ? "ok" : "failed", result, peerCount() > 0 ? "yes" : "no");
		int lastState = -100, failedState = 0;
		u64 doneAt = 0;
		for (;;) {
			u64 now = sceKernelGetSystemTimeWide();
			if (now - begin > 8000000 || (doneAt != 0 && now > doneAt)) {
				break;
			}
			if (listed < 0 && peerCount() > 0) {
				listed = (s64)(now - begin);
			}
			int state = ptpState(ptp);
			if (connected < 0 && state == 4) {
				connected = (s64)(now - begin);
				connectedFirst = listed < 0;
				int len = 4;
				if (sceNetAdhocPtpSend(ptp, "DATA", &len, 1000000, 0) == 0) {
					data = (s64)(sceKernelGetSystemTimeWide() - begin);
				}
			}
			// A connect that gave up, rather than one still trying.
			if (connected < 0 && lastState == 2 && state != 2 && state != 4) {
				failedState = 1;
			}
			lastState = state;
			if (doneAt == 0 && listed >= 0 && (connected >= 0 || failedState)) {
				doneAt = now + 300000;
			}
			sceKernelDelayThread(1000);
		}
		printf("Host in the peer list: %s\n", listed >= 0 ? "yes" : "never");
		printf("Connection established: %s\n", connected < 0 ? (failedState ? "no, it gave up" : "never") : (connectedFirst ? "yes, before the host was in the peer list" : "yes, after the host was in the peer list"));
		printf("Sent data on it: %s\n", data >= 0 ? "yes" : "no");
#ifdef SHOW_TIMES
		printf("[times] CONNECT event %lld us, listed %lld us, established %lld us, sent %lld us\n", joined, listed, connected, data);
#else
		(void)joined;
#endif
		sceKernelDelayThread(500000);
		sceNetAdhocPtpClose(ptp, 0);
	}
#endif
	pairShutdown();
	return 0;
}
