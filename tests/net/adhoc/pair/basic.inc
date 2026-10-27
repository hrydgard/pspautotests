// Two consoles in one group: the peer list, PDP and PTP between them, and what the host sees when
// the client leaves. Built as basic_host and basic_client.

#include "pair.h"

#define PDP_PORT 0x1000
#define PTP_PORT 0x2000

static u8 buffer[65536];

static void fillPattern(u8 *buf, int len, int seed) {
	for (int i = 0; i < len; ++i) {
		buf[i] = (u8)(i * 7 + seed);
	}
}

static int checkPattern(const u8 *buf, int len, int seed) {
	for (int i = 0; i < len; ++i) {
		if (buf[i] != (u8)(i * 7 + seed)) {
			return 0;
		}
	}
	return 1;
}

static void testPeers() {
	printf("\nPeers:\n");
	AdhocctlPeerInfo peers[4];
	memset(peers, 0, sizeof(peers));
	int size = sizeof(peers);
	int result = sceNetAdhocctlGetPeerList(&size, peers);
	printf("  GetPeerList: %08x, size %d\n", result, size);
	if (size >= (int)sizeof(AdhocctlPeerInfo)) {
		const AdhocctlPeerInfo *p = &peers[0];
		printf("  [0] next %s, mac %s, nickname %s, last_recv %s\n", p->next == NULL ? "NULL" : "set", pairMacName(&p->mac),
			p->nickname[0] ? "set" : "empty", p->last_recv != 0 ? "set" : "0");
	}
	size = 0x1234;
	printf("  GetPeerList size query: %08x, size %d\n", sceNetAdhocctlGetPeerList(&size, NULL), size);

	AdhocctlPeerInfo info;
	memset(&info, 0, sizeof(info));
	result = sceNetAdhocctlGetPeerInfo(&peerMac, sizeof(info), &info);
	printf("  GetPeerInfo: %08x, mac %s, nickname %s\n", result, pairMacName(&info.mac), strcmp(info.nickname, peers[0].nickname) == 0 ? "matches" : "differs");
	printf("  GetPeerInfo, size 4: %08x\n", sceNetAdhocctlGetPeerInfo(&peerMac, 4, &info));

	char nickname[128];
	memset(nickname, 0, sizeof(nickname));
	result = sceNetAdhocctlGetNameByAddr(&peerMac, nickname);
	printf("  GetNameByAddr peer: %08x, %s\n", result, strcmp(nickname, peers[0].nickname) == 0 ? "matches" : "differs");
	memset(nickname, 0, sizeof(nickname));
	printf("  GetNameByAddr local: %08x, %s\n", sceNetAdhocctlGetNameByAddr(&localMac, nickname), nickname[0] ? "set" : "empty");
	printf("  GetNameByAddr other: %08x\n", sceNetAdhocctlGetNameByAddr(&MAC_OTHER, nickname));

	AdhocctlPeerInfo byName[4];
	size = sizeof(byName);
	result = sceNetAdhocctlGetAddrByName(peers[0].nickname, &size, byName);
	printf("  GetAddrByName: %08x, size %d, %s\n", result, size, size >= (int)sizeof(AdhocctlPeerInfo) ? pairMacName(&byName[0].mac) : "-");

	// Both consoles see the same network.
	AdhocctlParameter param;
	sceNetAdhocctlGetParameter(&param);
	int pdp = sceNetAdhocPdpCreate(&localMac, PDP_PORT + 1, 0x1000, 0);
	pairSync(1);
	sceNetAdhocPdpSend(pdp, &peerMac, PDP_PORT + 1, &param.bssid, 6, 0, 1);
	AdhocMac other;
	int len = sizeof(other);
	AdhocMac from;
	u16 port;
	result = sceNetAdhocPdpRecv(pdp, &from, &port, &other, &len, 2000000, 0);
	printf("  Same BSSID as the peer: %s\n", result == 0 ? (memcmp(&other, &param.bssid, 6) == 0 ? "yes" : "no") : "no answer");
	sceNetAdhocPdpDelete(pdp, 0);
}

static void recvOne(const char *title, int id, int size) {
	AdhocMac from;
	memset(&from, 0xCC, sizeof(from));
	u16 port = 0xCCCC;
	int len = size;
	memset(buffer, 0, size);
	int result = sceNetAdhocPdpRecv(id, &from, &port, buffer, &len, 2000000, 0);
	printf("%s: %08x, len %d, from %s port %04x, data %s\n", title, result, len, pairMacName(&from), port,
		result == 0 ? (checkPattern(buffer, len, len & 0xFF) ? "ok" : "corrupt") : "-");
}

static void sendOne(const char *title, int id, const AdhocMac *mac, int len) {
	fillPattern(buffer, len, len & 0xFF);
	int result = sceNetAdhocPdpSend(id, mac, PDP_PORT, buffer, len, 1000000, 0);
	printf("%s: %08x\n", title, result);
}

static void testPdp() {
	printf("\nPDP:\n");
	int id = sceNetAdhocPdpCreate(&localMac, PDP_PORT, 0x20000, 0);
	printf("  Create: %s\n", id > 0 ? "ok" : "failed");
	pairSync(2);

	// 32000 bytes never arrive, though 16000 do. Sending 65523 bytes sometimes fails with 0x80410001
	// (out of memory), so it isn't tried here.
	static const int sizes[] = { 16, 1444, 1445, 4000, 8000, 16000, 32000 };
	for (int i = 0; i < (int)ARRAY_SIZE(sizes); ++i) {
		char title[64];
#if HOST
		snprintf(title, sizeof(title), "  Recv %d", sizes[i]);
		recvOne(title, id, 65536);
		snprintf(title, sizeof(title), "  Send %d back", sizes[i]);
		sendOne(title, id, &peerMac, sizes[i]);
#else
		snprintf(title, sizeof(title), "  Send %d", sizes[i]);
		sendOne(title, id, &peerMac, sizes[i]);
		snprintf(title, sizeof(title), "  Recv %d back", sizes[i]);
		recvOne(title, id, 65536);
#endif
	}

	pairSync(3);
#if HOST
	sendOne("  Broadcast 100", id, &MAC_BROADCAST, 100);
#else
	recvOne("  Recv broadcast", id, 65536);
#endif

	pairSync(4);
	// A burst of small datagrams, to see whether any are lost or reordered.
#if HOST
	int received = 0;
	int inOrder = 1;
	int last = -1;
	for (;;) {
		int len = 64;
		AdhocMac from;
		u16 port;
		if (sceNetAdhocPdpRecv(id, &from, &port, buffer, &len, 500000, 0) != 0) {
			break;
		}
		if (buffer[0] <= last) {
			inOrder = 0;
		}
		last = buffer[0];
		received++;
	}
	// Some are usually lost, and how many varies.
	printf("  Burst of 50: received %s, %s\n", received >= 40 ? "at least 40" : "fewer than 40", inOrder ? "in order" : "out of order");
#else
	int sent = 0;
	for (int i = 0; i < 50; ++i) {
		buffer[0] = (u8)i;
		if (sceNetAdhocPdpSend(id, &peerMac, PDP_PORT, buffer, 64, 1000000, 0) == 0) {
			sent++;
		}
	}
	printf("  Burst of 50: sent %d\n", sent);
#endif

	pairSync(5);
#if !HOST
	// Nobody on the host has this port open.
	fillPattern(buffer, 16, 0);
	printf("  Send to a closed port: %08x\n", sceNetAdhocPdpSend(id, &peerMac, PDP_PORT + 0x10, buffer, 16, 0, 1));
#endif
	sceNetAdhocPdpDelete(id, 0);
}

static const char *stateName(int state) {
	static const char *names[] = { "CLOSED", "LISTEN", "SYN_SENT", "SYN_RCVD", "ESTABLISHED" };
	return state >= 0 && state <= 4 ? names[state] : "?";
}

static void printPtpState(const char *title, int id) {
	PtpStat stats[4];
	int size = sizeof(stats);
	sceNetAdhocGetPtpStat(&size, stats);
	for (int i = 0; i < size / (int)sizeof(PtpStat) && i < 4; ++i) {
		if (stats[i].id == id) {
			printf("%s: peer %s port %04x, %s\n", title, pairMacName(&stats[i].peermac), stats[i].peerport, stateName(stats[i].state));
			return;
		}
	}
	printf("%s: not listed\n", title);
}

static void testPtp() {
	printf("\nPTP:\n");
#if HOST
	int listener = sceNetAdhocPtpListen(&localMac, PTP_PORT, 0x8000, 200000, 5, 1, 0);
	printf("  Listen: %s\n", listener > 0 ? "ok" : "failed");
	pairSync(6);
	AdhocMac from;
	u16 port = 0;
	int id = sceNetAdhocPtpAccept(listener, &from, &port, 5000000, 0);
	printf("  Accept: %s, from %s port %04x\n", id > 0 ? "ok" : "failed", pairMacName(&from), port);
	printPtpState("  Stat", id);

	// The client sends 20000 bytes in pieces of up to 4000.
	int total = 0;
	int ok = 1;
	while (total < 20000) {
		int len = 20000 - total;
		if (sceNetAdhocPtpRecv(id, buffer + total, &len, 3000000, 0) != 0) {
			break;
		}
		total += len;
	}
	ok = checkPattern(buffer, total, 0);
	printf("  Received %d bytes, %s\n", total, ok ? "ok" : "corrupt");
	fillPattern(buffer, 100, 1);
	int len = 100;
	printf("  Send 100: %08x, len %d\n", sceNetAdhocPtpSend(id, buffer, &len, 1000000, 0), len);
	pairSync(7);
	printf("  Close: %08x\n", sceNetAdhocPtpClose(id, 0));
	pairSync(8);
	pairSync(9);
	sceNetAdhocPtpClose(listener, 0);
#else
	pairSync(6);
	int id = sceNetAdhocPtpOpen(&localMac, PTP_PORT, &peerMac, PTP_PORT, 0x8000, 200000, 5, 0);
	printf("  Open: %s\n", id > 0 ? "ok" : "failed");
	printf("  Connect: %08x\n", sceNetAdhocPtpConnect(id, 5000000, 0));
	printPtpState("  Stat", id);

	fillPattern(buffer, 20000, 0);
	int total = 0;
	while (total < 20000) {
		int len = 20000 - total > 4000 ? 4000 : 20000 - total;
		if (sceNetAdhocPtpSend(id, buffer + total, &len, 3000000, 0) != 0) {
			break;
		}
		total += len;
	}
	printf("  Sent %d bytes\n", total);
	printf("  Flush: %08x\n", sceNetAdhocPtpFlush(id, 3000000, 0));
	int len = 100;
	memset(buffer, 0, 100);
	int result = sceNetAdhocPtpRecv(id, buffer, &len, 3000000, 0);
	printf("  Recv: %08x, len %d, %s\n", result, len, checkPattern(buffer, len, 1) ? "ok" : "corrupt");
	pairSync(7);
	// The host closes its end now.
	pairSync(8);
	len = 100;
	result = sceNetAdhocPtpRecv(id, buffer, &len, 3000000, 0);
	printf("  Recv after the host closed: %08x\n", result);
	printPtpState("  Stat", id);
	sceNetAdhocPtpClose(id, 0);

	// Nobody is listening on this port.
	int refused = sceNetAdhocPtpOpen(&localMac, PTP_PORT + 1, &peerMac, PTP_PORT + 1, 0x1000, 200000, 2, 0);
	u64 start = sceKernelGetSystemTimeWide();
	result = sceNetAdhocPtpConnect(refused, 5000000, 0);
	u64 took = sceKernelGetSystemTimeWide() - start;
	printf("  Connect to a closed port: %08x, %s\n", result, took < 1000000 ? "within a second" : "slow");
	sceNetAdhocPtpClose(refused, 0);
	pairSync(9);
#endif
}

static void testLeave() {
	printf("\nLeaving:\n");
#if HOST
	int start = eventCount;
	pairSync(10);
	// The client leaves now. See how long the host takes to notice, in whole seconds.
	int seconds = -1;
	for (int i = 0; i < 300; ++i) {
		int size = 0;
		sceNetAdhocctlGetPeerList(&size, NULL);
		if (size == 0) {
			seconds = i / 10;
			break;
		}
		sceKernelDelayThread(100000);
	}
	if (seconds < 0) {
		printf("  Peer still listed after 30 seconds\n");
	} else {
		printf("  Peer gone from the list after %s\n", seconds < 5 ? "less than 5 seconds" : (seconds < 15 ? "5-15 seconds" : "15-30 seconds"));
	}
	printf("  Events while waiting: %d\n", eventCount - start);
	int state = -1;
	sceNetAdhocctlGetState(&state);
	printf("  State: %d\n", state);
#else
	pairSync(10);
	adhocctlLeaveGroup();
#endif
}

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	testPeers();
	testPdp();
	testPtp();
	testLeave();
	pairShutdown();
	return 0;
}
