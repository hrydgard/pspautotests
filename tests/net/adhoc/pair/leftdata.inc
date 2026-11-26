// Whether reading a console's datagrams after it has left the group brings it back into the peer list.
// The client sends some datagrams and leaves at once; the host leaves them in its socket until the
// client is gone from its peer list, reads them, and then watches the peer list for two seconds.
// Built as leftdata_host and leftdata_client.

#include "pair.h"

#define DATA_PORT 0x7702

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
	pairSync(10);
	int sock = sceNetAdhocPdpCreate(&localMac, DATA_PORT, 0x2000, 0);
	pairSync(11);
#if HOST
	printf("Client gone from the peer list: %s\n", waitPeerCount(0, 5000) ? "yes" : "no");
	sceKernelDelayThread(100000);
	int fromPeer = 0;
	u8 buf[16];
	int len = sizeof(buf);
	AdhocMac mac;
	u16 port;
	while (sceNetAdhocPdpRecv(sock, &mac, &port, buf, &len, 0, 1) == 0) {
		if (memcmp(&mac, &peerMac, sizeof(mac)) == 0 && len == 4 && memcmp(buf, "LEFT", 4) == 0) {
			fromPeer++;
		}
		len = sizeof(buf);
	}
	printf("Its datagrams, read after that: %s\n", fromPeer >= 10 ? "10 or more of 20" : (fromPeer > 0 ? "fewer than 10 of 20" : "none"));
	printf("Client back in the peer list within 2 s: %s\n", waitPeerCount(1, 2000) ? "yes" : "no");
	AdhocctlPeerInfo info;
	printf("GetPeerInfo for it: %08x\n", sceNetAdhocctlGetPeerInfo(&peerMac, sizeof(info), &info));
#else
	for (int i = 0; i < 20; ++i) {
		sceNetAdhocPdpSend(sock, &peerMac, DATA_PORT, "LEFT", 4, 0, 1);
		sceKernelDelayThread(1000);
	}
	adhocctlLeaveGroup();
	// Stay away while the host watches.
	sceKernelDelayThread(6000000);
#endif
	sceNetAdhocPdpDelete(sock, 0);
	pairShutdown();
	return 0;
}
