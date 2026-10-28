// PTP connections between two consoles when one end goes away or isn't there yet: closing with data
// still unread, closing while the other end is blocked in a recv, a listener's backlog, and a connect
// made before the other side listens. Built as ptpclose_host and ptpclose_client.

#include "pair.h"

#define PTP_PORT 0x2400

static u8 buffer[4096];

static const char *elapsed(u64 us) {
	if (us < 1000000)
		return "under 1s";
	if (us < 2000000)
		return "1-2s";
	if (us < 4000000)
		return "2-4s";
	return "4s or more";
}

static void testUnreadData() {
	printf("\nClosing with data unread:\n");
#if HOST
	int listener = sceNetAdhocPtpListen(&localMac, PTP_PORT, 0x2000, 200000, 5, 1, 0);
	pairSync(1);
	int id = sceNetAdhocPtpAccept(listener, NULL, NULL, 5000000, 0);
	printf("  Accept: %s\n", id > 0 ? "ok" : "failed");
	// The client sends 3000 bytes and closes.
	pairSync(2);
	sceKernelDelayThread(500000);
	for (int i = 0; i < 4; ++i) {
		int len = 1000;
		int result = sceNetAdhocPtpRecv(id, buffer, &len, 1000000, 0);
		printf("  Recv %d: %08x, len %d\n", i, result, result == 0 ? len : -1);
		if (result != 0) {
			break;
		}
	}
	sceNetAdhocPtpClose(id, 0);
	sceNetAdhocPtpClose(listener, 0);
	pairSync(3);
#else
	pairSync(1);
	int id = sceNetAdhocPtpOpen(&localMac, PTP_PORT, &peerMac, PTP_PORT, 0x2000, 200000, 5, 0);
	printf("  Connect: %08x\n", sceNetAdhocPtpConnect(id, 5000000, 0));
	memset(buffer, 0x33, sizeof(buffer));
	int len = 3000;
	printf("  Send 3000: %08x, len %d\n", sceNetAdhocPtpSend(id, buffer, &len, 1000000, 0), len);
	printf("  Close: %08x\n", sceNetAdhocPtpClose(id, 0));
	pairSync(2);
	pairSync(3);
#endif
}

static void testBlockedRecv() {
	printf("\nClosing while the other end is blocked in recv:\n");
#if HOST
	int listener = sceNetAdhocPtpListen(&localMac, PTP_PORT + 1, 0x2000, 200000, 5, 1, 0);
	pairSync(4);
	int id = sceNetAdhocPtpAccept(listener, NULL, NULL, 5000000, 0);
	printf("  Accept: %s\n", id > 0 ? "ok" : "failed");
	pairSync(5);
	// The client closes about half a second from now.
	int len = sizeof(buffer);
	u64 start = sceKernelGetSystemTimeWide();
	int result = sceNetAdhocPtpRecv(id, buffer, &len, 4000000, 0);
	printf("  Blocking recv (4s timeout): %08x, returned %s\n", result, elapsed(sceKernelGetSystemTimeWide() - start));
	len = sizeof(buffer);
	printf("  Recv again: %08x\n", sceNetAdhocPtpRecv(id, buffer, &len, 0, 1));
	len = 16;
	printf("  Send: %08x\n", sceNetAdhocPtpSend(id, buffer, &len, 0, 1));
	sceNetAdhocPtpClose(id, 0);
	sceNetAdhocPtpClose(listener, 0);
	pairSync(6);
#else
	pairSync(4);
	int id = sceNetAdhocPtpOpen(&localMac, PTP_PORT + 1, &peerMac, PTP_PORT + 1, 0x2000, 200000, 5, 0);
	printf("  Connect: %08x\n", sceNetAdhocPtpConnect(id, 5000000, 0));
	pairSync(5);
	sceKernelDelayThread(500000);
	printf("  Close: %08x\n", sceNetAdhocPtpClose(id, 0));
	pairSync(6);
#endif
}

static void testBacklog() {
	printf("\nThree connects to a listener with backlog 1, nobody accepting:\n");
#if HOST
	int listener = sceNetAdhocPtpListen(&localMac, PTP_PORT + 2, 0x2000, 200000, 5, 1, 0);
	pairSync(7);
	// The client connects three sockets now.
	pairSync(8);
	int accepted = 0;
	for (int i = 0; i < 3; ++i) {
		int id = sceNetAdhocPtpAccept(listener, NULL, NULL, 1000000, 0);
		if (id > 0) {
			accepted++;
			sceNetAdhocPtpClose(id, 0);
		}
	}
	printf("  Accepted afterwards: %d\n", accepted);
	sceNetAdhocPtpClose(listener, 0);
	pairSync(9);
#else
	pairSync(7);
	int ids[3];
	for (int i = 0; i < 3; ++i) {
		ids[i] = sceNetAdhocPtpOpen(&localMac, PTP_PORT + 0x10 + i, &peerMac, PTP_PORT + 2, 0x2000, 200000, 5, 0);
		u64 start = sceKernelGetSystemTimeWide();
		int result = sceNetAdhocPtpConnect(ids[i], 2000000, 0);
		printf("  Connect %d (2s timeout): %08x, returned %s\n", i, result, elapsed(sceKernelGetSystemTimeWide() - start));
	}
	pairSync(8);
	pairSync(9);
	for (int i = 0; i < 3; ++i) {
		sceNetAdhocPtpClose(ids[i], 0);
	}
#endif
}

static void testConnectBeforeListen() {
	printf("\nConnecting a second before the other side listens:\n");
#if HOST
	pairSync(10);
	sceKernelDelayThread(1000000);
	int listener = sceNetAdhocPtpListen(&localMac, PTP_PORT + 3, 0x2000, 200000, 5, 1, 0);
	int id = sceNetAdhocPtpAccept(listener, NULL, NULL, 4000000, 0);
	printf("  Accept: %s\n", id > 0 ? "ok" : "nothing within 4s");
	if (id > 0) {
		sceNetAdhocPtpClose(id, 0);
	}
	sceNetAdhocPtpClose(listener, 0);
	pairSync(11);
#else
	int id = sceNetAdhocPtpOpen(&localMac, PTP_PORT + 3, &peerMac, PTP_PORT + 3, 0x2000, 200000, 5, 0);
	pairSync(10);
	u64 start = sceKernelGetSystemTimeWide();
	int result = sceNetAdhocPtpConnect(id, 4000000, 0);
	printf("  Connect (4s timeout): %08x, returned %s\n", result, elapsed(sceKernelGetSystemTimeWide() - start));
	if (result != 0) {
		// Try again, now that the host listens.
		sceKernelDelayThread(1000000);
		result = sceNetAdhocPtpConnect(id, 2000000, 0);
		printf("  Connect again: %08x\n", result);
	}
	sceNetAdhocPtpClose(id, 0);
	pairSync(11);
#endif
}

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	testUnreadData();
	testBlockedRecv();
	testBacklog();
	testConnectBeforeListen();
	pairShutdown();
	return 0;
}
