// Flow control between two consoles: how much PTP data a sender can queue while the receiver doesn't
// read, what PDP's buffer size limits on the receiving side, and what GetPdpStat and GetPtpStat count.
// Built as flow_host and flow_client.

#include "pairmatching.h"

#define PTP_PORT 0x2300
#define PDP_PORT 0x1300

static u8 buffer[8192];

static const char *sizeBucket(int bytes) {
	if (bytes == 0)
		return "0";
	if (bytes < 4096)
		return "under 4K";
	if (bytes < 8192)
		return "4K-8K";
	if (bytes < 16384)
		return "8K-16K";
	if (bytes < 32768)
		return "16K-32K";
	if (bytes < 65536)
		return "32K-64K";
	return "64K or more";
}

static inline int ptpRcv(int id) {
	PtpStat stats[8];
	int size = sizeof(stats);
	sceNetAdhocGetPtpStat(&size, stats);
	for (int i = 0; i < size / (int)sizeof(PtpStat) && i < 8; ++i) {
		if (stats[i].id == id) {
			return stats[i].rcv;
		}
	}
	return -1;
}

static inline int ptpSnd(int id) {
	PtpStat stats[8];
	int size = sizeof(stats);
	sceNetAdhocGetPtpStat(&size, stats);
	for (int i = 0; i < size / (int)sizeof(PtpStat) && i < 8; ++i) {
		if (stats[i].id == id) {
			return stats[i].snd;
		}
	}
	return -1;
}

static inline int pdpRcv(int id) {
	PdpStat stats[8];
	int size = sizeof(stats);
	sceNetAdhocGetPdpStat(&size, stats);
	for (int i = 0; i < size / (int)sizeof(PdpStat) && i < 8; ++i) {
		if (stats[i].id == id) {
			return stats[i].rcv;
		}
	}
	return -1;
}

static void testPtp() {
	printf("\nPTP, 8K buffers, receiver not reading:\n");
#if HOST
	int listener = sceNetAdhocPtpListen(&localMac, PTP_PORT, 0x2000, 200000, 5, 1, 0);
	pairSync(1);
	int id = sceNetAdhocPtpAccept(listener, NULL, NULL, 5000000, 0);
	printf("  Accept: %s\n", id > 0 ? "ok" : "failed");
	pairSync(2);
	// The client fills the connection now.
	pairSync(3);
	printf("  Bytes waiting (GetPtpStat): %s\n", sizeBucket(ptpRcv(id)));
	int total = 0;
	for (;;) {
		int len = sizeof(buffer);
		if (sceNetAdhocPtpRecv(id, buffer, &len, 1000000, 0) != 0) {
			break;
		}
		total += len;
	}
	printf("  Read: %s\n", sizeBucket(total));
	pairSync(4);
	sceNetAdhocPtpClose(id, 0);
	sceNetAdhocPtpClose(listener, 0);
#else
	pairSync(1);
	int id = sceNetAdhocPtpOpen(&localMac, PTP_PORT, &peerMac, PTP_PORT, 0x2000, 200000, 5, 0);
	printf("  Connect: %08x\n", sceNetAdhocPtpConnect(id, 5000000, 0));
	pairSync(2);
	int total = 0;
	int result = 0;
	memset(buffer, 0x5A, sizeof(buffer));
	for (int i = 0; i < 500; ++i) {
		int len = 1000;
		result = sceNetAdhocPtpSend(id, buffer, &len, 0, 1);
		if (result != 0) {
			// Give the data in flight a moment, then try once more.
			sceKernelDelayThread(200000);
			len = 1000;
			result = sceNetAdhocPtpSend(id, buffer, &len, 0, 1);
			if (result != 0) {
				break;
			}
		}
		total += len;
	}
	printf("  Sent before it stopped: %s, then %08x\n", sizeBucket(total), result);
	printf("  Bytes not yet sent (GetPtpStat): %s\n", sizeBucket(ptpSnd(id)));
	int len = 1000;
	printf("  Blocking send, 200ms: %08x\n", sceNetAdhocPtpSend(id, buffer, &len, 200000, 0));
	pairSync(3);
	// The host drains it now.
	sceKernelDelayThread(500000);
	len = 1000;
	printf("  Send once drained: %08x\n", sceNetAdhocPtpSend(id, buffer, &len, 1000000, 0));
	pairSync(4);
	sceNetAdhocPtpClose(id, 0);
#endif
}

static void testPdp() {
	printf("\nPDP, receiving buffer sizes:\n");
	static const int sizes[] = { 0, 2048, 0x8000 };
	for (int s = 0; s < 3; ++s) {
#if HOST
		int id = sceNetAdhocPdpCreate(&localMac, PDP_PORT, sizes[s], 0);
		pairSync(10 + s * 2);
		// The client sends 10 datagrams of 1000 bytes now.
		pairSync(11 + s * 2);
		sceKernelDelayThread(200000);
		int waiting = pdpRcv(id);
		int count = 0;
		for (;;) {
			AdhocMac from;
			u16 port;
			int len = sizeof(buffer);
			if (sceNetAdhocPdpRecv(id, &from, &port, buffer, &len, 0, 1) != 0) {
				break;
			}
			count++;
		}
		printf("  Buffer %d: GetPdpStat %s, received %s of 10\n", sizes[s], sizeBucket(waiting),
			count == 10 ? "all" : (count >= 8 ? "8-9" : (count == 0 ? "none" : "1-7")));
		sceNetAdhocPdpDelete(id, 0);
#else
		int id = sceNetAdhocPdpCreate(&localMac, PDP_PORT, 0x8000, 0);
		pairSync(10 + s * 2);
		int sent = 0;
		memset(buffer, 0x33, 1000);
		for (int i = 0; i < 10; ++i) {
			if (sceNetAdhocPdpSend(id, &peerMac, PDP_PORT, buffer, 1000, 0, 1) == 0) {
				sent++;
			}
			// Spaced out a little, so the radio isn't what loses them.
			sceKernelDelayThread(20000);
		}
		printf("  Buffer %d: sent %d\n", sizes[s], sent);
		pairSync(11 + s * 2);
		sceNetAdhocPdpDelete(id, 0);
#endif
	}
}

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	testPtp();
	testPdp();
	pairShutdown();
	return 0;
}
