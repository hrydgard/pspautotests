// Calls on TCP sockets in the wrong state, beyond net/inet/errors: listen before and after bind,
// connect on a listening socket and after a failed listen, connecting twice, shutdown's how, and
// how descriptor numbers are handed out again after a close.

#include "inet.h"

#define PORT 24500

static int tcp() {
	return sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
}

int main(int argc, char *argv[]) {
	if (!inetStartup()) {
		return 1;
	}
	InetAddr bound = inetMakeAddr(LOOPBACK, PORT);
	InetAddr nobody = inetMakeAddr(LOOPBACK, PORT + 1);
	InetAddr any = inetMakeAddr(0, PORT + 2);

	printf("listen:\n");
	int l = tcp();
	inetPrint("  Bound to loopback", (sceNetInetBind(l, &bound, sizeof(bound)), sceNetInetListen(l, 1)));
	inetPrint("  Again", sceNetInetListen(l, 4));
	inetPrint("  connect while listening", sceNetInetConnect(l, &nobody, sizeof(nobody)));
	inetPrint("  bind while listening", sceNetInetBind(l, &nobody, sizeof(nobody)));
	int s = tcp();
	inetPrint("  Unbound", sceNetInetListen(s, 1));
	inetPrint("  bind afterwards", sceNetInetBind(s, &any, sizeof(any)));
	inetPrint("  listen after the bind", sceNetInetListen(s, 1));
	sceNetInetClose(s);
	s = tcp();
	sceNetInetListen(s, 1);
	inetPrint("  connect to a listener after a failed listen", sceNetInetConnect(s, &bound, sizeof(bound)));
	sceNetInetClose(s);
	s = tcp();
	inetPrint("  connect to a listener, fresh socket", sceNetInetConnect(s, &bound, sizeof(bound)));
	inetPrint("  connect again", sceNetInetConnect(s, &bound, sizeof(bound)));
	inetPrint("  connect elsewhere", sceNetInetConnect(s, &nobody, sizeof(nobody)));
	inetPrint("  listen while connected", sceNetInetListen(s, 1));
	int server = sceNetInetAccept(l, NULL, NULL);
	sceNetInetClose(server);
	sceNetInetClose(s);

	printf("\nNon-blocking connect:\n");
	s = tcp();
	int one = 1;
	sceNetInetSetsockopt(s, INET_SOL_SOCKET, INET_SO_NBIO, &one, sizeof(one));
	inetPrint("  connect", sceNetInetConnect(s, &bound, sizeof(bound)));
	inetPrint("  connect again at once", sceNetInetConnect(s, &bound, sizeof(bound)));
	inetWait(s, 1, 500);
	inetPrint("  connect after it's writable", sceNetInetConnect(s, &bound, sizeof(bound)));
	server = sceNetInetAccept(l, NULL, NULL);
	sceNetInetClose(server);
	sceNetInetClose(s);

	printf("\nshutdown:\n");
	s = tcp();
	for (int how = -1; how <= 3; ++how) {
		char title[64];
		snprintf(title, sizeof(title), "  Not connected, how %d", how);
		inetPrint(title, sceNetInetShutdown(s, how));
	}
	sceNetInetClose(s);
	s = tcp();
	sceNetInetConnect(s, &bound, sizeof(bound));
	server = sceNetInetAccept(l, NULL, NULL);
	inetPrint("  Connected, how 3", sceNetInetShutdown(s, 3));
	inetPrint("  Connected, SHUT_WR", sceNetInetShutdown(s, INET_SHUT_WR));
	inetPrint("  send after SHUT_WR", sceNetInetSend(s, "x", 1, 0));
	inetPrint("  SHUT_WR again", sceNetInetShutdown(s, INET_SHUT_WR));
	char buf[8];
	inetPrint("  The peer's recv", sceNetInetRecv(server, buf, sizeof(buf), 0));
	inetPrint("  SHUT_RD", sceNetInetShutdown(s, INET_SHUT_RD));
	inetPrint("  recv after SHUT_RD", sceNetInetRecv(s, buf, sizeof(buf), INET_MSG_DONTWAIT));
	sceNetInetClose(server);
	sceNetInetClose(s);
	sceNetInetClose(l);

	printf("\nDescriptor numbers:\n");
	int a = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	sceNetInetClose(a);
	int b = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	printf("  Close, then a new one: %+d\n", b - a);
	int c = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	printf("  And another: %+d\n", c - b);
	sceNetInetClose(b);
	sceNetInetClose(c);
	// Open and close until the numbers come round again.
	int first = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	sceNetInetClose(first);
	int prev = first, steps = 0, wrapped = -1;
	for (steps = 1; steps < 600; ++steps) {
		int n = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
		sceNetInetClose(n);
		if (n < prev) {
			wrapped = n;
			break;
		}
		prev = n;
	}
	printf("  Highest before wrapping: %d, then %d\n", prev, wrapped);
	// With one held open at the wrap point, is it skipped?
	int held = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	int next = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	printf("  Two at once after wrapping: %d, %d\n", held, next);
	sceNetInetClose(held);
	sceNetInetClose(next);

	inetShutdown();
	return 0;
}
