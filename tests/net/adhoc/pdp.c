// sceNetAdhoc PDP sockets on one console, without joining a network: argument checking in
// PdpCreate, PdpSend, PdpRecv and PdpDelete, the receive timeouts, and GetPdpStat.

#include "adhoc.h"

#define PORT 0x1234

static u8 buffer[65536];

// Socket IDs keep increasing, so print how far each one is from the previous one.
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

static void testCreate(const char *title, const AdhocMac *mac, int port, int bufsize, int flag) {
	int result = sceNetAdhocPdpCreate(mac, port, bufsize, flag);
	printId(title, result);
	if (result > 0) {
		sceNetAdhocPdpDelete(result, 0);
	}
}

static void testSend(const char *title, int id, const AdhocMac *mac, int port, const void *data, int len, int flag) {
	int result = sceNetAdhocPdpSend(id, mac, port, data, len, 1000, flag);
	printf("%s: %08x\n", title, result);
}

static void testRecv(const char *title, int id, int withMac, int withPort, void *buf, int *len, u32 timeout, int flag) {
	AdhocMac mac;
	u16 port = 0xCCCC;
	int before = len ? *len : -1;
	int result = sceNetAdhocPdpRecv(id, withMac ? &mac : NULL, withPort ? &port : NULL, buf, len, timeout, flag);
	printf("%s: %08x, len %d -> %d\n", title, result, before, len ? *len : -1);
}

// Waits up to 100ms for a datagram on id, with a buffer of size bytes.
static void recvSelf(int id, int size) {
	AdhocMac mac;
	memset(&mac, 0xCC, sizeof(mac));
	u16 port = 0xCCCC;
	int len = size;
	memset(buffer, 0, size);
	int result = sceNetAdhocPdpRecv(id, &mac, &port, buffer, &len, 100000, 0);
	printf("  Recv into %d: %08x, len %d, from %s port %04x, data %02x\n", size, result, len, macName(&mac), port, buffer[0]);
}

static void printStat(const char *title) {
	int size = 0;
	int result = sceNetAdhocGetPdpStat(&size, NULL);
	printf("%s: size query %08x, size %d\n", title, result, size);

	PdpStat stats[4];
	memset(stats, 0xCC, sizeof(stats));
	size = sizeof(stats);
	result = sceNetAdhocGetPdpStat(&size, stats);
	printf("  stat %08x, size %d\n", result, size);
	for (int i = 0; i < size / (int)sizeof(PdpStat) && i < 4; ++i) {
		const PdpStat *s = &stats[i];
		int next = s->next == NULL ? -1 : (s->next == &stats[i + 1] ? i + 1 : -2);
		printf("  [%d] next %d, mac %s, port %04x, rcv %d\n", i, next, macName(&s->mac), s->port, (int)s->rcv);
	}
}

static void runTests() {
	printf("\nPdpCreate:\n");
	testCreate("  Normal", &localMac, PORT, 1024, 0);
	testCreate("  NULL mac", NULL, PORT, 1024, 0);
	testCreate("  Zero mac", &MAC_ZERO, PORT, 1024, 0);
	testCreate("  Broadcast mac", &MAC_BROADCAST, PORT, 1024, 0);
	testCreate("  Other mac", &MAC_OTHER, PORT, 1024, 0);
	testCreate("  Port 0", &localMac, 0, 1024, 0);
	testCreate("  Port 0xFFFF", &localMac, 0xFFFF, 1024, 0);
	testCreate("  Bufsize 0", &localMac, PORT, 0, 0);
	testCreate("  Bufsize 1", &localMac, PORT, 1, 0);
	testCreate("  Bufsize -1", &localMac, PORT, -1, 0);
	testCreate("  Bufsize 0x100000", &localMac, PORT, 0x100000, 0);
	testCreate("  Flag 1", &localMac, PORT, 1024, 1);
	testCreate("  Flag 2", &localMac, PORT, 1024, 2);

	int id = sceNetAdhocPdpCreate(&localMac, PORT, 0x2000, 0);
	printId("  Socket", id);
	testCreate("  Same port again", &localMac, PORT, 1024, 0);
	int id2 = sceNetAdhocPdpCreate(&localMac, PORT + 1, 0x2000, 0);
	printId("  Second socket", id2);

	printf("\nPdpSend:\n");
	testSend("  Broadcast", id, &MAC_BROADCAST, PORT, buffer, 16, 1);
	testSend("  Other mac", id, &MAC_OTHER, PORT, buffer, 16, 1);
	testSend("  Self", id, &localMac, PORT + 1, buffer, 16, 1);
	testSend("  Invalid id", 0, &MAC_BROADCAST, PORT, buffer, 16, 1);
	testSend("  Unused id", 100, &MAC_BROADCAST, PORT, buffer, 16, 1);
	testSend("  NULL mac", id, NULL, PORT, buffer, 16, 1);
	testSend("  Zero mac", id, &MAC_ZERO, PORT, buffer, 16, 1);
	testSend("  Multicast mac", id, &MAC_MULTICAST, PORT, buffer, 16, 1);
	testSend("  Port 0", id, &MAC_BROADCAST, 0, buffer, 16, 1);
	testSend("  Port 0xFFFF", id, &MAC_BROADCAST, 0xFFFF, buffer, 16, 1);
	testSend("  Length 0", id, &MAC_BROADCAST, PORT, buffer, 0, 1);
	testSend("  Length 0, NULL data", id, &MAC_BROADCAST, PORT, NULL, 0, 1);
	testSend("  Length 16, NULL data", id, &MAC_BROADCAST, PORT, NULL, 16, 1);
	testSend("  Length -1", id, &MAC_BROADCAST, PORT, buffer, -1, 1);
	testSend("  Length 1444", id, &MAC_BROADCAST, PORT, buffer, 1444, 1);
	testSend("  Length 1445", id, &MAC_BROADCAST, PORT, buffer, 1445, 1);
	testSend("  Length 65523", id, &MAC_BROADCAST, PORT, buffer, 65523, 1);
	testSend("  Length 65524", id, &MAC_BROADCAST, PORT, buffer, 65524, 1);
	testSend("  Flag 0", id, &MAC_BROADCAST, PORT, buffer, 16, 0);
	testSend("  Flag 2", id, &MAC_BROADCAST, PORT, buffer, 16, 2);
	testSend("  Flag 3", id, &MAC_BROADCAST, PORT, buffer, 16, 3);

	printf("\nReceiving from self:\n");
	recvSelf(id2, 64);
	memset(buffer, 0x55, 100);
	testSend("  Send 100 to self", id, &localMac, PORT + 1, buffer, 100, 1);
	recvSelf(id2, 10);
	recvSelf(id2, 128);
	recvSelf(id2, 128);
	testSend("  Broadcast 16", id2, &MAC_BROADCAST, PORT + 1, buffer, 16, 1);
	recvSelf(id2, 64);

	printf("\nPdpRecv:\n");
	int len = 64;
	testRecv("  Non-blocking, empty", id, 1, 1, buffer, &len, 0, 1);
	len = 64;
	testRecv("  Blocking, 1ms", id, 1, 1, buffer, &len, 1000, 0);
	len = 64;
	testRecv("  Invalid id", 0, 1, 1, buffer, &len, 0, 1);
	len = 64;
	testRecv("  Unused id", 100, 1, 1, buffer, &len, 0, 1);
	len = 64;
	testRecv("  NULL mac", id, 0, 1, buffer, &len, 0, 1);
	len = 64;
	testRecv("  NULL port", id, 1, 0, buffer, &len, 0, 1);
	len = 64;
	testRecv("  NULL buffer", id, 1, 1, NULL, &len, 0, 1);
	testRecv("  NULL length", id, 1, 1, buffer, NULL, 0, 1);
	len = 0;
	testRecv("  Length 0", id, 1, 1, buffer, &len, 0, 1);
	len = -1;
	testRecv("  Length -1", id, 1, 1, buffer, &len, 0, 1);
	len = 64;
	testRecv("  Flag 2", id, 1, 1, buffer, &len, 0, 2);

	printf("\nGetPdpStat:\n");
	printStat("  Two sockets");
	int size = 0;
	printf("  NULL size: %08x\n", sceNetAdhocGetPdpStat(NULL, NULL));
	size = 4;
	PdpStat small[1];
	printf("  Size 4: %08x", sceNetAdhocGetPdpStat(&size, small));
	printf(", size %d\n", size);

	printf("\nPdpDelete:\n");
	printf("  Invalid id: %08x\n", sceNetAdhocPdpDelete(0, 0));
	printf("  Unused id: %08x\n", sceNetAdhocPdpDelete(100, 0));
	printf("  Flag 1: %08x\n", sceNetAdhocPdpDelete(id2, 1));
	printf("  Normal: %08x\n", sceNetAdhocPdpDelete(id, 0));
	printf("  Twice: %08x\n", sceNetAdhocPdpDelete(id, 0));
	printStat("  No sockets");

}

int main(int argc, char *argv[]) {
	if (!adhocStartup()) {
		return 1;
	}

	AdhocMac mac;
	printf("sceNetGetLocalEtherAddr before adhocctl: %08x\n", sceNetGetLocalEtherAddr(&mac));
	testCreate("Create before init", &mac, PORT, 1024, 0);
	printf("sceNetAdhocInit: %08x\n", sceNetAdhocInit());
	printf("sceNetAdhocInit again: %08x\n", sceNetAdhocInit());
	if (!adhocctlStartup()) {
		adhocShutdown();
		return 1;
	}

	printf("\n== Outside a network ==\n");
	runTests();

	printf("\n== In a group ==\n");
	if (adhocctlCreateGroup("PDPTEST")) {
		AdhocMac mac;
		printf("sceNetGetLocalEtherAddr: %08x, %s\n", sceNetGetLocalEtherAddr(&mac), macName(&mac));
		runTests();
		adhocctlLeaveGroup();
	}

	printf("\nTerm:\n");
	printf("  sceNetAdhocTerm: %08x\n", sceNetAdhocTerm());
	printf("  sceNetAdhocTerm again: %08x\n", sceNetAdhocTerm());
	testCreate("  Create after term", &localMac, PORT, 1024, 0);

	adhocShutdown();
	return 0;
}
