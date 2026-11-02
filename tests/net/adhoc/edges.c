// Odds and ends PPSSPP's ad hoc code was unsure about, on one console in a group of its own: whether a
// PTP create that fails on a port in use uses up a socket ID, negative buffer sizes, SetSocketAlert with
// flags it doesn't know, a blocking connect to a peer that isn't there, a blocking broadcast, a second
// thread blocking on a socket another thread is blocked on, PtpAccept's flag, leaving a group with no
// handler registered, and PTP sockets outside a group.

#include "adhoc.h"

#define PORT 0x6800

static const AdhocMac MAC_NOBODY = { { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 } };

static const char *timeBucket(u64 us) {
	if (us < 50000)
		return "under 50 ms";
	if (us < 500000)
		return "50-500 ms";
	if (us < 1500000)
		return "0.5-1.5 s";
	return "over 1.5 s";
}

static const char *idOrError(int result, int prev) {
	static char buf[32];
	if (result < 0) {
		snprintf(buf, sizeof(buf), "%08x", result);
	} else {
		snprintf(buf, sizeof(buf), "socket, id +%d", result - prev);
	}
	return buf;
}

static volatile int threadResult;
static volatile int threadDone;
static int threadSock;

static int recvThread(SceSize argc, void *argv) {
	u8 buf[64];
	int len = sizeof(buf);
	threadResult = sceNetAdhocPtpRecv(threadSock, buf, &len, 1000000, 0);
	threadDone = 1;
	return 0;
}

static void testPortInUse() {
	printf("\nPort in use:\n");
	int l1 = sceNetAdhocPtpListen(&localMac, PORT, 4096, 200000, 5, 1, 0);
	int again = sceNetAdhocPtpListen(&localMac, PORT, 4096, 200000, 5, 1, 0);
	int l2 = sceNetAdhocPtpListen(&localMac, PORT + 1, 4096, 200000, 5, 1, 0);
	printf("  PtpListen same port: %08x, next listen: %s\n", again, idOrError(l2, l1));

	int o1 = sceNetAdhocPtpOpen(&localMac, PORT + 2, &MAC_NOBODY, PORT, 4096, 200000, 5, 0);
	again = sceNetAdhocPtpOpen(&localMac, PORT + 2, &MAC_NOBODY, PORT, 4096, 200000, 5, 0);
	int o2 = sceNetAdhocPtpOpen(&localMac, PORT + 3, &MAC_NOBODY, PORT, 4096, 200000, 5, 0);
	printf("  PtpOpen same port and peer: %08x, next open: %s\n", again, idOrError(o2, o1));

	int b1 = sceNetAdhocPtpOpen(&localMac, PORT + 4, &MAC_NOBODY, PORT, -1, 200000, 5, 0);
	printf("  PtpOpen bufsize -1: %s\n", idOrError(b1, o2));
	int b2 = sceNetAdhocPtpListen(&localMac, PORT + 5, -1, 200000, 5, 1, 0);
	printf("  PtpListen bufsize -1: %s\n", idOrError(b2, b1 > 0 ? b1 : o2));

	int ids[] = { l1, l2, o1, o2, b1, b2 };
	for (int i = 0; i < 6; ++i) {
		if (ids[i] > 0) {
			sceNetAdhocPtpClose(ids[i], 0);
		}
	}
}

static void testAlertFlags() {
	printf("\nSetSocketAlert flags:\n");
	int sock = sceNetAdhocPdpCreate(&localMac, PORT, 1024, 0);
	unsigned int tests[] = { 0x1, 0x10, 0x80, 0x400, 0x1000, 0xFFFFFFFF, 0x7FF0 };
	for (int i = 0; i < 7; ++i) {
		int result = sceNetAdhocSetSocketAlert(sock, tests[i]);
		int flags = -1;
		sceNetAdhocGetSocketAlert(sock, &flags);
		printf("  %08x: %08x, flags now %08x\n", tests[i], result, flags);
	}
	sceNetAdhocSetSocketAlert(sock, 0);
	sceNetAdhocPdpDelete(sock, 0);
}

static void testConnectNobody() {
	printf("\nConnecting to a MAC that isn't there:\n");
	int sock = sceNetAdhocPtpOpen(&localMac, 0, &MAC_NOBODY, PORT, 4096, 200000, 5, 0);
	u64 t = sceKernelGetSystemTimeWide();
	int result = sceNetAdhocPtpConnect(sock, 1000000, 0);
	printf("  Blocking, 1 s timeout: %08x, %s\n", result, timeBucket(sceKernelGetSystemTimeWide() - t));
	result = sceNetAdhocPtpConnect(sock, 0, 1);
	printf("  Then non-blocking: %08x\n", result);
	sceNetAdhocPtpClose(sock, 0);

	sock = sceNetAdhocPtpOpen(&localMac, 0, &MAC_NOBODY, PORT, 4096, 200000, 5, 0);
	t = sceKernelGetSystemTimeWide();
	result = sceNetAdhocPtpConnect(sock, 0, 1);
	printf("  Non-blocking first: %08x\n", result);
	int last = result;
	while (sceKernelGetSystemTimeWide() - t < 3000000) {
		sceKernelDelayThread(10000);
		result = sceNetAdhocPtpConnect(sock, 0, 1);
		if (result != last) {
			printf("  Then %08x after %s\n", result, timeBucket(sceKernelGetSystemTimeWide() - t));
			last = result;
		}
	}
	printf("  Still %08x after 3 s\n", last);
	sceNetAdhocPtpClose(sock, 0);
}

static void testBroadcast() {
	printf("\nBlocking broadcast, alone in the group:\n");
	int sock = sceNetAdhocPdpCreate(&localMac, PORT, 1024, 0);
	u8 data[16] = { 0 };
	u64 t = sceKernelGetSystemTimeWide();
	int result = sceNetAdhocPdpSend(sock, &MAC_BROADCAST, PORT, data, sizeof(data), 100000, 0);
	printf("  PdpSend 16, 100 ms timeout: %08x, %s\n", result, timeBucket(sceKernelGetSystemTimeWide() - t));
	result = sceNetAdhocPdpSend(sock, &MAC_BROADCAST, PORT, data, sizeof(data), 0, 0);
	printf("  PdpSend 16, no timeout: %08x\n", result);
	sceNetAdhocPdpDelete(sock, 0);
}

static void testSecondBlocker() {
	printf("\nTwo threads blocked on one socket:\n");
	int listener = sceNetAdhocPtpListen(&localMac, PORT + 10, 4096, 200000, 5, 1, 0);
	int client = sceNetAdhocPtpOpen(&localMac, PORT + 11, &localMac, PORT + 10, 4096, 200000, 5, 0);
	sceNetAdhocPtpConnect(client, 1000000, 1);
	int server = sceNetAdhocPtpAccept(listener, NULL, NULL, 1000000, 0);
	sceNetAdhocPtpConnect(client, 1000000, 0);
	printf("  Connected: %s\n", server > 0 ? "yes" : "no");

	threadSock = server;
	threadDone = 0;
	threadResult = 0x1234;
	SceUID thread = sceKernelCreateThread("blocked", recvThread, 0x18, 0x4000, 0, NULL);
	sceKernelStartThread(thread, 0, NULL);
	sceKernelDelayThread(50000);
	u8 buf[64];
	int len = sizeof(buf);
	u64 t = sceKernelGetSystemTimeWide();
	int result = sceNetAdhocPtpRecv(server, buf, &len, 200000, 0);
	printf("  Second blocking recv, 200 ms: %08x, %s\n", result, timeBucket(sceKernelGetSystemTimeWide() - t));
	len = sizeof(buf);
	result = sceNetAdhocPtpRecv(server, buf, &len, 0, 1);
	printf("  Non-blocking recv meanwhile: %08x\n", result);
	sceKernelWaitThreadEnd(thread, NULL);
	sceKernelDeleteThread(thread);
	printf("  First thread: %08x\n", threadResult);

	t = sceKernelGetSystemTimeWide();
	result = sceNetAdhocPtpAccept(listener, NULL, NULL, 200000, 2);
	printf("  PtpAccept with flag 2, nothing waiting: %08x, %s\n", result, timeBucket(sceKernelGetSystemTimeWide() - t));
	t = sceKernelGetSystemTimeWide();
	result = sceNetAdhocPtpAccept(listener, NULL, NULL, 200000, -1);
	printf("  PtpAccept with flag -1: %08x, %s\n", result, timeBucket(sceKernelGetSystemTimeWide() - t));

	sceNetAdhocPtpClose(client, 0);
	sceNetAdhocPtpClose(server, 0);
	sceNetAdhocPtpClose(listener, 0);
}

static void testNoHandler() {
	printf("\nLeaving with no handler:\n");
	sceNetAdhocctlDelHandler(handlerId);
	handlerId = -1;
	int result = sceNetAdhocctlDisconnect();
	int state = -1;
	sceNetAdhocctlGetState(&state);
	printf("  Disconnect: %08x, state right after %d\n", result, state);
	sceKernelDelayThread(10000);
	sceNetAdhocctlGetState(&state);
	printf("  After 10 ms: %d\n", state);
	sceKernelDelayThread(1000000);
	sceNetAdhocctlGetState(&state);
	printf("  After 1 s: %d\n", state);

	printf("\nCreating with no handler:\n");
	result = sceNetAdhocctlCreate("EDGES2");
	sceNetAdhocctlGetState(&state);
	printf("  Create: %08x, state right after %d\n", result, state);
	int s = state;
	u64 t = sceKernelGetSystemTimeWide();
	while (sceKernelGetSystemTimeWide() - t < 5000000 && s != 1) {
		sceKernelDelayThread(10000);
		sceNetAdhocctlGetState(&s);
	}
	printf("  State %d, %s\n", s, timeBucket(sceKernelGetSystemTimeWide() - t));
	sceNetAdhocctlDisconnect();
	t = sceKernelGetSystemTimeWide();
	while (sceKernelGetSystemTimeWide() - t < 5000000 && s != 0) {
		sceKernelDelayThread(10000);
		sceNetAdhocctlGetState(&s);
	}
	printf("  Disconnect: state %d, %s\n", s, timeBucket(sceKernelGetSystemTimeWide() - t));
}

static void testOutsideGroup() {
	printf("\nOutside a group:\n");
	int l = sceNetAdhocPtpListen(&localMac, PORT + 20, 4096, 200000, 5, 1, 0);
	printf("  PtpListen: %s\n", l > 0 ? "socket" : "error");
	if (l < 0) {
		printf("    %08x\n", l);
	}
	int o = sceNetAdhocPtpOpen(&localMac, PORT + 21, &MAC_NOBODY, PORT, 4096, 200000, 5, 0);
	printf("  PtpOpen: %s\n", o > 0 ? "socket" : "error");
	if (o < 0) {
		printf("    %08x\n", o);
	}
	if (l > 0) {
		sceNetAdhocPtpClose(l, 0);
	}
	if (o > 0) {
		sceNetAdhocPtpClose(o, 0);
	}
}

int main(int argc, char *argv[]) {
	if (!adhocStartup()) {
		return 1;
	}
	printf("sceNetAdhocInit: %08x\n", sceNetAdhocInit());
	if (!adhocctlStartup() || !adhocctlCreateGroup("EDGES")) {
		adhocShutdown();
		return 1;
	}

	testPortInUse();
	testAlertFlags();
	testConnectNobody();
	testBroadcast();
	testSecondBlocker();
	testNoHandler();
	testOutsideGroup();

	printf("\n");
	adhocShutdown();
	return 0;
}
