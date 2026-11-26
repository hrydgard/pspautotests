// Whether a console's datagrams get through before that console is in the peer list. The host creates
// a group and sends a datagram every 5 ms, the client scans for the group, joins and does the same once
// it's connected. Each side notes when the other showed up in its peer list and when the other's first
// datagram came. Kurohyou 2 gives up on a room when it has had the host's datagrams for half a second
// without the host being in its peer list. One round broadcasts, the other sends to the peer's address.
// Both start in the pair group, and go back to it between rounds. Built as joindata_host and
// joindata_client.

// Define SHOW_TIMES to also print the times, which differ from run to run.
#include "pair.h"

#define DATA_GROUP "JOINDATA"
#define DATA_PORT 0x7701

static int adhocctlState() {
	int state = -1;
	int result = sceNetAdhocctlGetState(&state);
	return result < 0 ? result : state;
}

static int peerCount() {
	AdhocctlPeerInfo peers[4];
	int size = sizeof(peers);
	if (sceNetAdhocctlGetPeerList(&size, peers) < 0) {
		return -1;
	}
	return size / (int)sizeof(AdhocctlPeerInfo);
}

static void leaveIfInGroup() {
	int state = adhocctlState();
	if (state == 1) {
		adhocctlLeaveGroup();
	} else if (state != 0) {
		sceNetAdhocctlDisconnect();
		sceKernelDelayThread(500000);
	}
}

// Each case takes the same time on both consoles, however soon either side is done.
static void waitUntil(u64 begin, int seconds) {
	u64 end = begin + (u64)seconds * 1000000;
	u64 now = sceKernelGetSystemTimeWide();
	if (now < end) {
		sceKernelDelayThread((SceUInt)(end - now));
	}
}

static void backToPairGroup(int step) {
	leaveIfInGroup();
	sceKernelDelayThread(2000000);
	if (pairJoin() && pairFindPeer() == 1) {
		pairSync(step);
	} else {
		printf("  Couldn't get back to the pair group\n");
	}
}

typedef struct {
	// Microseconds after `begin`, or -1.
	s64 peerListed;
	s64 firstData;
	int dataBeforeListed;
	int dataAfterListed;
	int sendErrors;
	int firstSendError;
} Exchange;

// Sends a datagram every 5 ms and takes what arrives, every millisecond, until the peer has been in the
// peer list and sending for half a second, or for timeoutMs.
static void exchange(int sock, int broadcast, u64 begin, int timeoutMs, Exchange *x) {
	x->peerListed = -1;
	x->firstData = -1;
	x->dataBeforeListed = 0;
	x->dataAfterListed = 0;
	x->sendErrors = 0;
	x->firstSendError = 0;
	u64 start = sceKernelGetSystemTimeWide();
	u64 doneAt = 0;
	for (int i = 0; ; ++i) {
		u64 now = sceKernelGetSystemTimeWide();
		if (now - start > (u64)timeoutMs * 1000 || (doneAt != 0 && now > doneAt)) {
			break;
		}
		if (x->peerListed < 0 && peerCount() > 0) {
			x->peerListed = (s64)(now - begin);
		}
		if (i % 5 == 0) {
			int result = sceNetAdhocPdpSend(sock, broadcast ? &MAC_BROADCAST : &peerMac, DATA_PORT, "DATA", 4, 0, 1);
			if (result < 0) {
				if (x->sendErrors++ == 0) {
					x->firstSendError = result;
				}
			}
		}
		u8 buf[16];
		int len = sizeof(buf);
		AdhocMac mac;
		u16 port;
		while (sceNetAdhocPdpRecv(sock, &mac, &port, buf, &len, 0, 1) == 0) {
			if (memcmp(&mac, &peerMac, sizeof(mac)) == 0) {
				if (x->firstData < 0) {
					x->firstData = (s64)(sceKernelGetSystemTimeWide() - begin);
				}
				if (x->peerListed < 0) {
					x->dataBeforeListed++;
				} else {
					x->dataAfterListed++;
				}
			}
			len = sizeof(buf);
		}
		if (doneAt == 0 && x->peerListed >= 0 && x->firstData >= 0) {
			doneAt = now + 500000;
		}
		sceKernelDelayThread(1000);
	}
}

static void printExchange(const char *who, const Exchange *x) {
	printf("  %s in the peer list: %s\n", who, x->peerListed >= 0 ? "yes" : "never");
	printf("  Datagrams from it before that: %s\n", x->dataBeforeListed == 0 ? "none" : "some");
	printf("  Datagrams from it after that: %s\n", x->dataAfterListed == 0 ? "none" : "some");
	if (x->sendErrors != 0) {
		printf("  Sends failed, the first with %08x\n", x->firstSendError);
	} else {
		printf("  No send failed\n");
	}
#ifdef SHOW_TIMES
	printf("  [times] listed %lld us, first datagram %lld us, %d before, %d after, %d send errors\n",
		x->peerListed, x->firstData, x->dataBeforeListed, x->dataAfterListed, x->sendErrors);
#endif
}

static void joinData(int broadcast, int step) {
	pairSync(step);
	u64 caseBegin = sceKernelGetSystemTimeWide();
	leaveIfInGroup();
	printf("\n%s:\n", broadcast ? "Broadcasting" : "Sending to the peer's address");
	Exchange x;
	int sock = -1;
#if HOST
	sceKernelDelayThread(500000);
	int start = eventCount;
	sceNetAdhocctlCreate(DATA_GROUP);
	waitEvent(start, 5000);
	sock = sceNetAdhocPdpCreate(&localMac, DATA_PORT, 0x2000, 0);
	exchange(sock, broadcast, sceKernelGetSystemTimeWide(), 9000, &x);
	printExchange("Client", &x);
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
				if (memcmp(infos[i].group, DATA_GROUP, 8) == 0) {
					info = infos[i];
					found = 1;
				}
			}
		}
	}
	if (!found) {
		printf("  Group not found\n");
	} else {
		int start = eventCount;
		u64 begin = sceKernelGetSystemTimeWide();
		sceNetAdhocctlJoin(&info);
		int event = waitEvent(start, 5000);
		s64 connected = (s64)(sceKernelGetSystemTimeWide() - begin);
		printf("  Join: event %d\n", event);
		// Like a game, the socket is made once connected.
		sock = sceNetAdhocPdpCreate(&localMac, DATA_PORT, 0x2000, 0);
		exchange(sock, broadcast, begin, 6000, &x);
		printExchange("Host", &x);
#ifdef SHOW_TIMES
		printf("  [times] CONNECT event %lld us after Join\n", connected);
#else
		(void)connected;
#endif
	}
#endif
	if (sock >= 0) {
		sceNetAdhocPdpDelete(sock, 0);
	}
	leaveIfInGroup();
	waitUntil(caseBegin, 16);
	backToPairGroup(step + 1);
}

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	joinData(1, 10);
	joinData(0, 12);
	pairShutdown();
	return 0;
}
