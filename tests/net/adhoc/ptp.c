// sceNetAdhoc PTP sockets on one console, in a group of its own: argument checking in PtpOpen
// and PtpListen, a connection to itself, and the calls on sockets in the wrong state.

#include "adhoc.h"

#define PORT 0x2345

static u8 buffer[4096];

static int lastId = 0;

static void printId(const char *title, int result) {
	if (result > 0) {
		if (lastId == 0) {
			printf("%s: socket\n", title);
		} else {
			printf("%s: socket, id +%d\n", title, result - lastId);
		}
		lastId = result;
	} else {
		printf("%s: %08x\n", title, result);
	}
}

static void testOpen(const char *title, const AdhocMac *mac, int port, const AdhocMac *dst, int dport, int bufsize, u32 rexmt, int count, int flag) {
	int result = sceNetAdhocPtpOpen(mac, port, dst, dport, bufsize, rexmt, count, flag);
	printId(title, result);
	if (result > 0) {
		sceNetAdhocPtpClose(result, 0);
	}
}

static void testListen(const char *title, const AdhocMac *mac, int port, int bufsize, u32 rexmt, int count, int backlog, int flag) {
	int result = sceNetAdhocPtpListen(mac, port, bufsize, rexmt, count, backlog, flag);
	printId(title, result);
	if (result > 0) {
		sceNetAdhocPtpClose(result, 0);
	}
}

static const char *stateName(int state) {
	static const char *names[] = { "CLOSED", "LISTEN", "SYN_SENT", "SYN_RCVD", "ESTABLISHED" };
	return state >= 0 && state <= 4 ? names[state] : "?";
}

static void printStat(const char *title, int a, int b, int c) {
	PtpStat stats[4];
	memset(stats, 0xCC, sizeof(stats));
	int size = sizeof(stats);
	int result = sceNetAdhocGetPtpStat(&size, stats);
	printf("%s: %08x, size %d\n", title, result, size);
	for (int i = 0; i < size / (int)sizeof(PtpStat) && i < 4; ++i) {
		const PtpStat *s = &stats[i];
		const char *which = s->id == a ? "listener" : (s->id == b ? "client" : (s->id == -1 ? "id -1" : (s->id == c ? "accepted" : "?")));
		printf("  %s: mac %s, port %04x, peer %s port %04x, snd %d, rcv %d, %s\n", which, macName(&s->mac), s->port,
			macName(&s->peermac), s->peerport, (int)s->snd, (int)s->rcv, stateName(s->state));
	}
}

int main(int argc, char *argv[]) {
	if (!adhocStartup()) {
		return 1;
	}
	printf("sceNetAdhocInit: %08x\n", sceNetAdhocInit());
	if (!adhocctlStartup() || !adhocctlCreateGroup("PTPTEST")) {
		adhocShutdown();
		return 1;
	}

	printf("\nPtpOpen:\n");
	testOpen("  Normal", &localMac, PORT, &MAC_OTHER, PORT, 1024, 200000, 5, 0);
	testOpen("  NULL mac", NULL, PORT, &MAC_OTHER, PORT, 1024, 200000, 5, 0);
	testOpen("  Other mac", &MAC_OTHER, PORT, &MAC_OTHER, PORT, 1024, 200000, 5, 0);
	testOpen("  NULL destination", &localMac, PORT, NULL, PORT, 1024, 200000, 5, 0);
	testOpen("  Zero destination", &localMac, PORT, &MAC_ZERO, PORT, 1024, 200000, 5, 0);
	testOpen("  Broadcast destination", &localMac, PORT, &MAC_BROADCAST, PORT, 1024, 200000, 5, 0);
	testOpen("  Port 0", &localMac, 0, &MAC_OTHER, PORT, 1024, 200000, 5, 0);
	testOpen("  Destination port 0", &localMac, PORT, &MAC_OTHER, 0, 1024, 200000, 5, 0);
	testOpen("  Destination port 0xFFFF", &localMac, PORT, &MAC_OTHER, 0xFFFF, 1024, 200000, 5, 0);
	testOpen("  Bufsize 0", &localMac, PORT, &MAC_OTHER, PORT, 0, 200000, 5, 0);
	testOpen("  Rexmt 0", &localMac, PORT, &MAC_OTHER, PORT, 1024, 0, 5, 0);
	testOpen("  Count 0", &localMac, PORT, &MAC_OTHER, PORT, 1024, 200000, 0, 0);
	testOpen("  Flag 1", &localMac, PORT, &MAC_OTHER, PORT, 1024, 200000, 5, 1);

	printf("\nPtpListen:\n");
	testListen("  Normal", &localMac, PORT, 1024, 200000, 5, 1, 0);
	testListen("  NULL mac", NULL, PORT, 1024, 200000, 5, 1, 0);
	testListen("  Port 0", &localMac, 0, 1024, 200000, 5, 1, 0);
	testListen("  Bufsize 0", &localMac, PORT, 0, 200000, 5, 1, 0);
	testListen("  Rexmt 0", &localMac, PORT, 1024, 0, 5, 1, 0);
	testListen("  Count 0", &localMac, PORT, 1024, 200000, 0, 1, 0);
	testListen("  Backlog 0", &localMac, PORT, 1024, 200000, 5, 0, 0);
	testListen("  Flag 1", &localMac, PORT, 1024, 200000, 5, 1, 1);

	printf("\nConnecting to itself:\n");
	int listener = sceNetAdhocPtpListen(&localMac, PORT, 4096, 200000, 5, 1, 0);
	printId("  Listen", listener);
	int len = 16;
	printf("  Send on listener: %08x\n", sceNetAdhocPtpSend(listener, buffer, &len, 0, 1));
	AdhocMac mac;
	u16 port = 0;
	printf("  Accept, nothing waiting: %08x\n", sceNetAdhocPtpAccept(listener, &mac, &port, 0, 1));
	printf("  Accept, 1ms: %08x\n", sceNetAdhocPtpAccept(listener, &mac, &port, 1000, 0));

	int client = sceNetAdhocPtpOpen(&localMac, PORT + 1, &localMac, PORT, 4096, 200000, 5, 0);
	printId("  Open", client);
	printStat("  Stat after open", listener, client, -1);
	printf("  Accept on client: %08x\n", sceNetAdhocPtpAccept(client, &mac, &port, 0, 1));
	printf("  Connect: %08x\n", sceNetAdhocPtpConnect(client, 2000000, 0));
	memset(&mac, 0xCC, sizeof(mac));
	port = 0xCCCC;
	int accepted = sceNetAdhocPtpAccept(listener, &mac, &port, 2000000, 0);
	printId("  Accept", accepted);
	printf("    from %s port %04x\n", macName(&mac), port);
	printf("  Connect again: %08x\n", sceNetAdhocPtpConnect(client, 0, 1));
	printStat("  Stat after accept", listener, client, accepted);

	printf("\nData:\n");
	memset(buffer, 0x55, sizeof(buffer));
	len = 100;
	printf("  Send 100: %08x, len %d\n", sceNetAdhocPtpSend(client, buffer, &len, 0, 1), len);
	printf("  Flush: %08x\n", sceNetAdhocPtpFlush(client, 1000000, 0));
	memset(buffer, 0, sizeof(buffer));
	len = 30;
	printf("  Recv 30: %08x, len %d, data %02x\n", sceNetAdhocPtpRecv(accepted, buffer, &len, 1000000, 0), len, buffer[0]);
	len = 1000;
	printf("  Recv 1000: %08x, len %d\n", sceNetAdhocPtpRecv(accepted, buffer, &len, 1000000, 0), len);
	len = 1000;
	printf("  Recv empty: %08x, len %d\n", sceNetAdhocPtpRecv(accepted, buffer, &len, 0, 1), len);
	len = 1000;
	printf("  Recv, 1ms: %08x, len %d\n", sceNetAdhocPtpRecv(accepted, buffer, &len, 1000, 0), len);
	len = 16;
	printf("  Send NULL data: %08x\n", sceNetAdhocPtpSend(client, NULL, &len, 0, 1));
	printf("  Send NULL length: %08x\n", sceNetAdhocPtpSend(client, buffer, NULL, 0, 1));
	len = 0;
	printf("  Send 0: %08x, len %d\n", sceNetAdhocPtpSend(client, buffer, &len, 0, 1), len);
	len = -1;
	printf("  Send -1: %08x, len %d\n", sceNetAdhocPtpSend(client, buffer, &len, 0, 1), len);
	len = 16;
	printf("  Recv on listener: %08x\n", sceNetAdhocPtpRecv(listener, buffer, &len, 0, 1));

	printf("\nClose:\n");
	printf("  Close accepted: %08x\n", sceNetAdhocPtpClose(accepted, 0));
	sceKernelDelayThread(100000);
	len = 16;
	printf("  Recv on client: %08x, len %d\n", sceNetAdhocPtpRecv(client, buffer, &len, 500000, 0), len);
	len = 16;
	printf("  Send on client: %08x\n", sceNetAdhocPtpSend(client, buffer, &len, 0, 1));
	printStat("  Stat", listener, client, accepted);
	printf("  Close client: %08x\n", sceNetAdhocPtpClose(client, 0));
	printf("  Close twice: %08x\n", sceNetAdhocPtpClose(client, 0));
	printf("  Close flag 1: %08x\n", sceNetAdhocPtpClose(listener, 1));
	printf("  Close listener: %08x\n", sceNetAdhocPtpClose(listener, 0));

	printf("\nConnecting to nobody:\n");
	client = sceNetAdhocPtpOpen(&localMac, PORT + 2, &localMac, PORT + 3, 4096, 200000, 2, 0);
	printId("  Open", client);
	printf("  Connect, non-blocking: %08x\n", sceNetAdhocPtpConnect(client, 0, 1));
	printf("  Connect: %08x\n", sceNetAdhocPtpConnect(client, 2000000, 0));
	sceNetAdhocPtpClose(client, 0);

	printf("\n");
	adhocctlLeaveGroup();
	adhocShutdown();
	return 0;
}
