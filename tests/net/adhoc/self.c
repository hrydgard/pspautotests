// Whether one console can talk to itself: PDP broadcasts between two of its own sockets, and a
// matching host and client in the same process.

#include "adhoc.h"

#define PORT 0x4567

static volatile int clientEvents[8];
static volatile int clientEventCount = 0;
static volatile int hostEvents[8];
static volatile int hostEventCount = 0;

static void clientHandler(int id, int event, AdhocMac *mac, int optlen, void *opt) {
	if (clientEventCount < 8) {
		clientEvents[clientEventCount] = event;
	}
	clientEventCount++;
}

static void hostHandler(int id, int event, AdhocMac *mac, int optlen, void *opt) {
	if (hostEventCount < 8) {
		hostEvents[hostEventCount] = event;
	}
	hostEventCount++;
}

static void printMatchingEvents() {
	printf("  host events (%d):", hostEventCount);
	for (int i = 0; i < hostEventCount && i < 8; ++i) {
		printf(" %d", hostEvents[i]);
	}
	printf("\n  client events (%d):", clientEventCount);
	for (int i = 0; i < clientEventCount && i < 8; ++i) {
		printf(" %d", clientEvents[i]);
	}
	printf("\n");
}

static void testMatching(const char *title, int clientPort) {
	printf("\n%s:\n", title);
	hostEventCount = 0;
	clientEventCount = 0;
	int host = sceNetAdhocMatchingCreate(1, 4, PORT, 1024, 200000, 500000, 5, 200000, &hostHandler);
	int client = sceNetAdhocMatchingCreate(2, 2, clientPort, 1024, 200000, 500000, 5, 200000, &clientHandler);
	printf("  Create host: %s, client: %s\n", host > 0 ? "ok" : "failed", client > 0 ? "ok" : "failed");
	if (client < 0) {
		printf("  Client create: %08x\n", client);
	}
	if (host > 0) {
		printf("  Start host: %08x\n", sceNetAdhocMatchingStart(host, 0x30, 0x2000, 0x30, 0x2000, 4, "helo"));
	}
	if (client > 0) {
		printf("  Start client: %08x\n", sceNetAdhocMatchingStart(client, 0x30, 0x2000, 0x30, 0x2000, 0, NULL));
	}
	// Hellos go out every 200ms.
	sceKernelDelayThread(2000000);
	printMatchingEvents();
	if (client > 0) {
		sceNetAdhocMatchingStop(client);
		sceNetAdhocMatchingDelete(client);
	}
	if (host > 0) {
		sceNetAdhocMatchingStop(host);
		sceNetAdhocMatchingDelete(host);
	}
}

int main(int argc, char *argv[]) {
	if (!adhocStartup()) {
		return 1;
	}
	printf("sceNetAdhocInit: %08x\n", sceNetAdhocInit());
	if (!adhocctlStartup() || !adhocctlCreateGroup("SELFTEST")) {
		adhocShutdown();
		return 1;
	}

	printf("\nPDP between two sockets:\n");
	int a = sceNetAdhocPdpCreate(&localMac, PORT, 0x2000, 0);
	int b = sceNetAdhocPdpCreate(&localMac, PORT + 1, 0x2000, 0);
	printf("  Create: %s, %s\n", a > 0 ? "ok" : "failed", b > 0 ? "ok" : "failed");
	char buf[64];
	int sent = sceNetAdhocPdpSend(a, &MAC_BROADCAST, PORT + 1, "broadcast", 10, 0, 1);
	AdhocMac mac;
	u16 port = 0;
	int len = sizeof(buf);
	memset(buf, 0, sizeof(buf));
	int result = sceNetAdhocPdpRecv(b, &mac, &port, buf, &len, 200000, 0);
	printf("  Broadcast to the other socket: sent %08x, recv %08x, '%s'\n", sent, result, buf);
	sent = sceNetAdhocPdpSend(a, &localMac, PORT + 1, "unicast", 8, 0, 1);
	len = sizeof(buf);
	memset(buf, 0, sizeof(buf));
	result = sceNetAdhocPdpRecv(b, &mac, &port, buf, &len, 200000, 0);
	printf("  Unicast to the other socket: sent %08x, recv %08x, '%s'\n", sent, result, buf);
	sceNetAdhocPdpDelete(a, 0);
	sceNetAdhocPdpDelete(b, 0);

	printf("\nsceNetAdhocMatchingInit: %08x\n", sceNetAdhocMatchingInit(0x20000));
	testMatching("Matching, host and client on the same port", PORT);
	testMatching("Matching, client on another port", PORT + 1);
	sceNetAdhocMatchingTerm();

	printf("\n");
	adhocctlLeaveGroup();
	adhocShutdown();
	return 0;
}
