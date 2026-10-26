// Binding UDP sockets, and exchanging datagrams between them through the loopback interface.
// A socket bound to INADDR_ANY or to port 0 keeps those in getsockname(), bind() leaves the
// address it was passed alone, and the source address recvfrom() reports is the sender's binding.

#include <common.h>
#include <pspkernel.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <psputility.h>
#include <psputility_netmodules.h>
#include <netinet/in.h>
#include <string.h>

#define PORT_ANY_ADDR 23460
#define PORT_LOOPBACK 23461

// The PSP's fd_set has 256 bits. Newlib's is smaller, and select() would write past it.
typedef struct {
	u32 bits[8];
} InetFdSet;
#define INET_FD_SET(fd, set) ((set)->bits[(fd) >> 5] |= 1U << ((fd) & 31))

static void fillAddr(struct sockaddr_in *addr, u32 ip, u16 port) {
	memset(addr, 0, sizeof(*addr));
	addr->sin_len = sizeof(*addr);
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	addr->sin_addr.s_addr = htonl(ip);
}

static void printAddr(const struct sockaddr_in *addr) {
	const u8 *ip = (const u8 *)&addr->sin_addr.s_addr;
	printf("%d.%d.%d.%d:%d", ip[0], ip[1], ip[2], ip[3], ntohs(addr->sin_port));
}

static void testBind(const char *title, int sock, u32 ip, u16 port) {
	struct sockaddr_in addr, orig;
	fillAddr(&addr, ip, port);
	orig = addr;
	int result = sceNetInetBind(sock, (struct sockaddr *)&addr, sizeof(addr));
	printf("%s: %d, errno %d, address %s\n", title, result, result < 0 ? sceNetInetGetErrno() : 0, memcmp(&addr, &orig, sizeof(addr)) == 0 ? "unchanged" : "changed");
}

static void testGetsockname(const char *title, int sock) {
	struct sockaddr_in addr;
	memset(&addr, 0xCC, sizeof(addr));
	socklen_t len = sizeof(addr);
	int result = sceNetInetGetsockname(sock, (struct sockaddr *)&addr, &len);
	const u8 *ip = (const u8 *)&addr.sin_addr.s_addr;
	u16 port = ntohs(addr.sin_port);
	// A port picked by the stack varies, so only say whether there is one.
	printf("%s: %d, len %d, family %d, %d.%d.%d.%d:%s\n", title, result, (int)len, addr.sin_family, ip[0], ip[1], ip[2], ip[3],
		port == PORT_ANY_ADDR || port == PORT_LOOPBACK ? "requested port" : (port == 0 ? "0" : "some port"));
}

static void testExchange(const char *title, int from, int to, u16 toPort) {
	struct sockaddr_in addr;
	fillAddr(&addr, 0x7F000001, toPort);
	int sent = sceNetInetSendto(from, title, strlen(title), 0, (struct sockaddr *)&addr, sizeof(addr));

	// Waits with a generous timeout: an emulator can run emulated time far ahead of the host's network.
	InetFdSet readfds;
	memset(&readfds, 0, sizeof(readfds));
	INET_FD_SET(to, &readfds);
	struct SceNetInetTimeval tv = { 1, 0 };
	int ready = sceNetInetSelect(to + 1, (fd_set *)&readfds, NULL, NULL, &tv);

	char buf[64];
	memset(buf, 0, sizeof(buf));
	memset(&addr, 0xCC, sizeof(addr));
	socklen_t len = sizeof(addr);
	int received = -1;
	if (ready > 0) {
		received = sceNetInetRecvfrom(to, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&addr, &len);
	}
	printf("%s: sent %d, ready %d, received %d '%s' from ", title, sent, ready, received, buf);
	printAddr(&addr);
	printf("\n");
}

int main(int argc, char *argv[]) {
	setvbuf(stdout, NULL, _IONBF, 0);
	printf("load common: %08x\n", sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON));
	printf("load inet: %08x\n", sceUtilityLoadNetModule(PSP_NET_MODULE_INET));
	int result = sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000);
	printf("sceNetInit: %08x\n", result);
	if (result < 0) {
		return 1;
	}
	result = sceNetInetInit();
	printf("sceNetInetInit: %08x\n", result);
	if (result < 0) {
		sceNetTerm();
		return 1;
	}

	int anyAddr = sceNetInetSocket(AF_INET, SOCK_DGRAM, 0);
	int loopback = sceNetInetSocket(AF_INET, SOCK_DGRAM, 0);
	int anyPort = sceNetInetSocket(AF_INET, SOCK_DGRAM, 0);
	printf("sockets: %s\n", anyAddr >= 0 && loopback >= 0 && anyPort >= 0 ? "valid" : "invalid");

	testBind("bind INADDR_ANY", anyAddr, INADDR_ANY, PORT_ANY_ADDR);
	testGetsockname("getsockname, INADDR_ANY", anyAddr);
	testBind("bind 127.0.0.1", loopback, 0x7F000001, PORT_LOOPBACK);
	testGetsockname("getsockname, 127.0.0.1", loopback);
	testBind("bind INADDR_ANY, port 0", anyPort, INADDR_ANY, 0);
	testGetsockname("getsockname, port 0", anyPort);

	testExchange("loopback to INADDR_ANY", loopback, anyAddr, PORT_ANY_ADDR);
	testExchange("INADDR_ANY to loopback", anyAddr, loopback, PORT_LOOPBACK);

	sceNetInetClose(anyPort);
	sceNetInetClose(loopback);
	sceNetInetClose(anyAddr);
	sceNetInetTerm();
	sceNetTerm();
	sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
	return 0;
}
