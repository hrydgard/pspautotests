// Two sockets bound to the same port: what SO_REUSEADDR and SO_REUSEPORT allow, for UDP and TCP,
// on the same address and on INADDR_ANY alongside loopback.

#include "inet.h"

static int port = 24600;

static void testPair(const char *title, int type, u32 ip1, u32 ip2, int opt1, int opt2, int listenFirst) {
	int a = sceNetInetSocket(INET_AF_INET, type, 0);
	int b = sceNetInetSocket(INET_AF_INET, type, 0);
	int one = 1;
	if (opt1) {
		sceNetInetSetsockopt(a, INET_SOL_SOCKET, opt1, &one, sizeof(one));
	}
	if (opt2) {
		sceNetInetSetsockopt(b, INET_SOL_SOCKET, opt2, &one, sizeof(one));
	}
	InetAddr addr1 = inetMakeAddr(ip1, port);
	InetAddr addr2 = inetMakeAddr(ip2, port);
	int r1 = sceNetInetBind(a, &addr1, sizeof(addr1));
	if (listenFirst) {
		sceNetInetListen(a, 1);
	}
	int r2 = sceNetInetBind(b, &addr2, sizeof(addr2));
	printf("%s: %d, then %d", title, r1, r2);
	if (r2 < 0) {
		printf(", errno %d", sceNetInetGetErrno());
	}
	printf("\n");
	sceNetInetClose(a);
	sceNetInetClose(b);
	port++;
}

int main(int argc, char *argv[]) {
	if (!inetStartup()) {
		return 1;
	}
	const int RA = INET_SO_REUSEADDR, RP = INET_SO_REUSEPORT;

	printf("UDP:\n");
	testPair("  Same address", INET_SOCK_DGRAM, LOOPBACK, LOOPBACK, 0, 0, 0);
	testPair("  Same address, REUSEADDR on the second", INET_SOCK_DGRAM, LOOPBACK, LOOPBACK, 0, RA, 0);
	testPair("  Same address, REUSEADDR on both", INET_SOCK_DGRAM, LOOPBACK, LOOPBACK, RA, RA, 0);
	testPair("  Same address, REUSEPORT on the second", INET_SOCK_DGRAM, LOOPBACK, LOOPBACK, 0, RP, 0);
	testPair("  Same address, REUSEPORT on both", INET_SOCK_DGRAM, LOOPBACK, LOOPBACK, RP, RP, 0);
	testPair("  ANY, then loopback", INET_SOCK_DGRAM, 0, LOOPBACK, 0, 0, 0);
	testPair("  Loopback, then ANY", INET_SOCK_DGRAM, LOOPBACK, 0, 0, 0, 0);
	testPair("  ANY, then loopback, REUSEADDR on the second", INET_SOCK_DGRAM, 0, LOOPBACK, 0, RA, 0);
	testPair("  ANY, then loopback, REUSEADDR on both", INET_SOCK_DGRAM, 0, LOOPBACK, RA, RA, 0);
	testPair("  ANY twice", INET_SOCK_DGRAM, 0, 0, 0, 0, 0);
	testPair("  ANY twice, REUSEADDR on both", INET_SOCK_DGRAM, 0, 0, RA, RA, 0);

	printf("\nTCP:\n");
	testPair("  Same address", INET_SOCK_STREAM, LOOPBACK, LOOPBACK, 0, 0, 0);
	testPair("  Same address, REUSEADDR on both", INET_SOCK_STREAM, LOOPBACK, LOOPBACK, RA, RA, 0);
	testPair("  Same address, REUSEPORT on both", INET_SOCK_STREAM, LOOPBACK, LOOPBACK, RP, RP, 0);
	testPair("  Same address, first listening", INET_SOCK_STREAM, LOOPBACK, LOOPBACK, 0, 0, 1);
	testPair("  ANY, then loopback", INET_SOCK_STREAM, 0, LOOPBACK, 0, 0, 0);
	testPair("  ANY, then loopback, REUSEADDR on the second", INET_SOCK_STREAM, 0, LOOPBACK, 0, RA, 0);
	testPair("  ANY, then loopback, REUSEADDR on both", INET_SOCK_STREAM, 0, LOOPBACK, RA, RA, 0);

	printf("\nTCP and UDP on the same port:\n");
	int t = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
	int u = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	InetAddr addr = inetMakeAddr(LOOPBACK, port);
	inetPrint("  TCP", sceNetInetBind(t, &addr, sizeof(addr)));
	inetPrint("  UDP", sceNetInetBind(u, &addr, sizeof(addr)));
	sceNetInetClose(t);
	sceNetInetClose(u);

	printf("\nAfter a close:\n");
	int a = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	addr = inetMakeAddr(LOOPBACK, port + 1);
	sceNetInetBind(a, &addr, sizeof(addr));
	sceNetInetClose(a);
	int b = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	inetPrint("  UDP, the same port", sceNetInetBind(b, &addr, sizeof(addr)));
	sceNetInetClose(b);

	inetShutdown();
	return 0;
}
