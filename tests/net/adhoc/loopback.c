// Whether sceNetInet's loopback (127.0.0.1) works alongside ad hoc: UDP and TCP to the console
// itself, before ad hoc, with adhocctl initialized, while in an ad hoc group, and after leaving it.

#include "adhoc.h"
#include <pspnet_inet.h>
#include <netinet/in.h>

int sceNetInetInit(void);
int sceNetInetTerm(void);

#define UDP_PORT 23470
#define TCP_PORT 23471

// The PSP's fd_set has 256 bits. Newlib's is smaller, and select() would write past it.
typedef struct {
	u32 bits[8];
} InetFdSet;

static void fillAddr(struct sockaddr_in *addr, u16 port) {
	memset(addr, 0, sizeof(*addr));
	addr->sin_len = sizeof(*addr);
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	addr->sin_addr.s_addr = htonl(0x7F000001);
}

// Waits up to a second for sock to become readable.
static int waitReadable(int sock) {
	InetFdSet fds;
	memset(&fds, 0, sizeof(fds));
	fds.bits[sock >> 5] |= 1U << (sock & 31);
	struct SceNetInetTimeval tv = { 1, 0 };
	return sceNetInetSelect(sock + 1, (fd_set *)&fds, NULL, NULL, &tv);
}

static void testUdp() {
	int sock = sceNetInetSocket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in addr;
	fillAddr(&addr, UDP_PORT);
	int bound = sceNetInetBind(sock, (struct sockaddr *)&addr, sizeof(addr));
	int sent = sceNetInetSendto(sock, "udp", 4, 0, (struct sockaddr *)&addr, sizeof(addr));
	int ready = waitReadable(sock);
	char buf[16] = {};
	int received = ready > 0 ? sceNetInetRecvfrom(sock, buf, sizeof(buf), 0, NULL, NULL) : -1;
	printf("  UDP: socket %s, bind %d, sent %d, ready %d, received %d '%s'\n", sock >= 0 ? "ok" : "failed", bound, sent, ready, received, buf);
	sceNetInetClose(sock);
}

// Each phase uses its own port, since a closed listener's port stays busy for a while.
static int tcpPort = TCP_PORT;

static void testTcp() {
	int listener = sceNetInetSocket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr;
	fillAddr(&addr, tcpPort++);
	int bound = sceNetInetBind(listener, (struct sockaddr *)&addr, sizeof(addr));
	int listened = sceNetInetListen(listener, 1);
	int client = sceNetInetSocket(AF_INET, SOCK_STREAM, 0);
	int connected = sceNetInetConnect(client, (struct sockaddr *)&addr, sizeof(addr));
	int connectErrno = connected < 0 ? sceNetInetGetErrno() : 0;
	int ready = waitReadable(listener);
	int accepted = ready > 0 ? sceNetInetAccept(listener, NULL, NULL) : -1;
	int sent = sceNetInetSend(client, "tcp", 4, 0);
	char buf[16] = {};
	int received = accepted >= 0 && waitReadable(accepted) > 0 ? sceNetInetRecv(accepted, buf, sizeof(buf), 0) : -1;
	printf("  TCP: bind %d, listen %d, connect %d (errno %d), accept %s, sent %d, received %d '%s'\n", bound, listened,
		connected, connectErrno, accepted >= 0 ? "ok" : "failed", sent, received, buf);
	if (accepted >= 0) {
		sceNetInetClose(accepted);
	}
	sceNetInetClose(client);
	sceNetInetClose(listener);
}

static void testLoopback(const char *title) {
	printf("%s:\n", title);
	testUdp();
	testTcp();
}

int main(int argc, char *argv[]) {
	if (!adhocStartup()) {
		return 1;
	}
	printf("load inet: %08x\n", sceUtilityLoadNetModule(PSP_NET_MODULE_INET));
	printf("sceNetInetInit: %08x\n", sceNetInetInit());

	testLoopback("\nBefore ad hoc");

	printf("\nsceNetAdhocInit: %08x\n", sceNetAdhocInit());
	if (!adhocctlStartup()) {
		adhocShutdown();
		return 1;
	}
	testLoopback("With adhocctl initialized");

	printf("\n");
	if (adhocctlCreateGroup("LOOPBACK")) {
		testLoopback("In an ad hoc group");
		adhocctlLeaveGroup();
	}

	testLoopback("\nAfter leaving the group");

	printf("\n");
	printf("sceNetAdhocctlTerm: %08x\n", sceNetAdhocctlTerm());
	printf("sceNetAdhocTerm: %08x\n", sceNetAdhocTerm());
	printf("sceNetInetTerm: %08x\n", sceNetInetTerm());
	printf("sceNetTerm: %08x\n", sceNetTerm());
	printf("unload inet: %08x\n", sceUtilityUnloadNetModule(PSP_NET_MODULE_INET));
	printf("unload adhoc: %08x\n", sceUtilityUnloadNetModule(PSP_NET_MODULE_ADHOC));
	printf("unload common: %08x\n", sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON));
	return 0;
}
