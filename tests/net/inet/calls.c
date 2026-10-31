// What the sceNetInet calls that timing and sizes leave out cost when they return without waiting:
// the address conversions, socket setup and teardown, socket options, the TCP calls over loopback,
// the PCB lists by the number of sockets, and calls on a bad socket. A measurement, not a
// regression test: the numbers differ from run to run.

#include "inet.h"

#define PORT 25200
#define REPEAT 30
#define TCP_REPEAT 10

static u64 timeNow() {
	return sceKernelGetSystemTimeWide();
}

static u32 samples[REPEAT];

static void sortU32(u32 *v, int n) {
	for (int i = 1; i < n; ++i) {
		u32 x = v[i];
		int j = i - 1;
		while (j >= 0 && v[j] > x) {
			v[j + 1] = v[j];
			--j;
		}
		v[j + 1] = x;
	}
}

static void printSamples(const char *title, int n) {
	sortU32(samples, n);
	printf("  %-44s min %5u med %5u us\n", title, (unsigned)samples[0], (unsigned)samples[n / 2]);
}

// Times `call` REPEAT times, running `setup` and `teardown` around each, outside the timing.
#define TIME_EACH(title, setup, call, teardown) TIME_N(REPEAT, title, setup, call, teardown)
#define TIME_N(n, title, setup, call, teardown) do { \
	for (int _r = 0; _r < (n); ++_r) { \
		setup; \
		u64 _t = timeNow(); \
		call; \
		samples[_r] = (u32)(timeNow() - _t); \
		teardown; \
	} \
	printSamples(title, (n)); \
} while (0)

static int udpSocket(u16 port) {
	int s = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	InetAddr addr = inetMakeAddr(LOOPBACK, port);
	sceNetInetBind(s, &addr, sizeof(addr));
	return s;
}

static u16 nextPort = PORT + 100;

// A new connection to the listener at la, without blocking for long if one doesn't come.
static int connectPair(int listener, const InetAddr *la, int *client, int *server) {
	*client = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
	int nb = 1;
	sceNetInetSetsockopt(*client, INET_SOL_SOCKET, INET_SO_NBIO, &nb, sizeof(nb));
	sceNetInetConnect(*client, la, sizeof(*la));
	*server = -1;
	if (inetWait(listener, 0, 500) > 0) {
		*server = sceNetInetAccept(listener, NULL, NULL);
	}
	inetWait(*client, 1, 500);
	nb = 0;
	sceNetInetSetsockopt(*client, INET_SOL_SOCKET, INET_SO_NBIO, &nb, sizeof(nb));
	if (*server < 0) {
		printf("  (no connection)\n");
		return 0;
	}
	return 1;
}

// With RST, so that nothing is left in TIME_WAIT: a PSP runs out after about 60 connections.
static void closePair(int client, int server) {
	if (client >= 0) {
		sceNetInetCloseWithRST(client);
	}
	if (server >= 0) {
		sceNetInetCloseWithRST(server);
	}
	sceKernelDelayThread(2000);
}

static void timeConversions() {
	printf("Address conversions:\n");
	u32 ip = 0;
	char str[32];
	u8 bin[4] = { 192, 168, 1, 20 };
	TIME_EACH("InetAddr", , sceNetInetInetAddr("192.168.1.20"), );
	TIME_EACH("InetAton", , sceNetInetInetAton("192.168.1.20", &ip), );
	TIME_EACH("InetPton", , sceNetInetInetPton(INET_AF_INET, "192.168.1.20", &ip), );
	TIME_EACH("InetNtop", , sceNetInetInetNtop(INET_AF_INET, bin, str, sizeof(str)), );
	TIME_EACH("InetAton, bad", , sceNetInetInetAton("not an address", &ip), );
	TIME_EACH("GetErrno", , sceNetInetGetErrno(), );
	TIME_EACH("GetPspError", , sceNetInetGetPspError(), );
}

static void timeSetup() {
	printf("\nSetup and teardown:\n");
	int s = -1;
	InetAddr addr;
	TIME_EACH("socket(UDP)", , s = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0), sceNetInetClose(s));
	TIME_EACH("socket(TCP)", , s = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0), sceNetInetClose(s));
	TIME_EACH("close(UDP, never used)", s = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0), sceNetInetClose(s), );
	TIME_EACH("close(UDP, bound)", s = udpSocket(PORT), sceNetInetClose(s), );
	TIME_EACH("CloseWithRST(TCP, never used)", s = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0), sceNetInetCloseWithRST(s), );
	TIME_EACH("bind(UDP)", (s = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0), addr = inetMakeAddr(LOOPBACK, PORT)), sceNetInetBind(s, &addr, sizeof(addr)), sceNetInetClose(s));
	TIME_EACH("bind(TCP)", (s = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0), addr = inetMakeAddr(LOOPBACK, nextPort++)), sceNetInetBind(s, &addr, sizeof(addr)), sceNetInetClose(s));
	TIME_EACH("bind(UDP, port 0)", (s = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0), addr = inetMakeAddr(LOOPBACK, 0)), sceNetInetBind(s, &addr, sizeof(addr)), sceNetInetClose(s));
	TIME_EACH("listen", (s = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0), addr = inetMakeAddr(LOOPBACK, nextPort++), sceNetInetBind(s, &addr, sizeof(addr))), sceNetInetListen(s, 4), sceNetInetClose(s));
	InetAddr peer = inetMakeAddr(LOOPBACK, PORT + 1);
	TIME_EACH("connect(UDP)", s = udpSocket(PORT), sceNetInetConnect(s, &peer, sizeof(peer)), sceNetInetClose(s));
	TIME_EACH("SocketAbort(UDP, idle)", s = udpSocket(PORT), sceNetInetSocketAbort(s), sceNetInetClose(s));
}

static void timeOptions() {
	printf("\nSocket options:\n");
	int udp = udpSocket(PORT);
	int tcp = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
	int one = 1, val = 0, len = 0, buf = 32768, ttl = 32;
	TIME_EACH("setsockopt(SO_REUSEADDR)", , sceNetInetSetsockopt(udp, INET_SOL_SOCKET, INET_SO_REUSEADDR, &one, sizeof(one)), );
	TIME_EACH("setsockopt(SO_NBIO)", , sceNetInetSetsockopt(udp, INET_SOL_SOCKET, INET_SO_NBIO, &one, sizeof(one)), );
	TIME_EACH("setsockopt(SO_RCVBUF)", , sceNetInetSetsockopt(udp, INET_SOL_SOCKET, INET_SO_RCVBUF, &buf, sizeof(buf)), );
	TIME_EACH("setsockopt(IP_TTL)", , sceNetInetSetsockopt(udp, INET_IPPROTO_IP, INET_IP_TTL, &ttl, sizeof(ttl)), );
	TIME_EACH("setsockopt(TCP_NODELAY)", , sceNetInetSetsockopt(tcp, INET_IPPROTO_TCP, INET_TCP_NODELAY, &one, sizeof(one)), );
	TIME_EACH("setsockopt(bad option)", , sceNetInetSetsockopt(udp, INET_SOL_SOCKET, 0x7777, &one, sizeof(one)), );
	TIME_EACH("getsockopt(SO_TYPE)", len = sizeof(val), sceNetInetGetsockopt(udp, INET_SOL_SOCKET, INET_SO_TYPE, &val, &len), );
	TIME_EACH("getsockopt(SO_ERROR)", len = sizeof(val), sceNetInetGetsockopt(udp, INET_SOL_SOCKET, INET_SO_ERROR, &val, &len), );
	TIME_EACH("getsockopt(SO_RCVBUF)", len = sizeof(val), sceNetInetGetsockopt(udp, INET_SOL_SOCKET, INET_SO_RCVBUF, &val, &len), );
	TIME_EACH("getsockopt(TCP_NODELAY)", len = sizeof(val), sceNetInetGetsockopt(tcp, INET_IPPROTO_TCP, INET_TCP_NODELAY, &val, &len), );
	InetAddr addr;
	TIME_EACH("getsockname(UDP)", len = sizeof(addr), sceNetInetGetsockname(udp, &addr, &len), );
	TIME_EACH("getpeername(UDP, not connected)", len = sizeof(addr), sceNetInetGetpeername(udp, &addr, &len), );
	sceNetInetClose(udp);
	sceNetInetClose(tcp);
}

static void timeTcp() {
	printf("\nTCP over loopback:\n");
	int listener = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
	InetAddr la = inetMakeAddr(LOOPBACK, PORT + 2);
	sceNetInetBind(listener, &la, sizeof(la));
	sceNetInetListen(listener, 8);
	int client = -1, server = -1, len = 0, nb = 1;
	InetAddr addr;
	// accept doesn't block: the listener doesn't either.
	sceNetInetSetsockopt(listener, INET_SOL_SOCKET, INET_SO_NBIO, &nb, sizeof(nb));

	// A connection waiting to be accepted for each.
	TIME_N(TCP_REPEAT, "accept, NULL address",
		(client = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0), sceNetInetSetsockopt(client, INET_SOL_SOCKET, INET_SO_NBIO, &nb, sizeof(nb)), sceNetInetConnect(client, &la, sizeof(la)), inetWait(listener, 0, 500)),
		server = sceNetInetAccept(listener, NULL, NULL),
		closePair(client, server));
	TIME_N(TCP_REPEAT, "accept, with an address",
		(client = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0), sceNetInetSetsockopt(client, INET_SOL_SOCKET, INET_SO_NBIO, &nb, sizeof(nb)), sceNetInetConnect(client, &la, sizeof(la)), inetWait(listener, 0, 500), len = sizeof(addr)),
		server = sceNetInetAccept(listener, &addr, &len),
		closePair(client, server));

	connectPair(listener, &la, &client, &server);
	TIME_N(TCP_REPEAT, "getsockname(TCP, connected)", len = sizeof(addr), sceNetInetGetsockname(client, &addr, &len), );
	TIME_N(TCP_REPEAT, "getpeername(TCP, connected)", len = sizeof(addr), sceNetInetGetpeername(client, &addr, &len), );
	int val = 0;
	TIME_N(TCP_REPEAT, "getsockopt(SO_ERROR, connected)", len = sizeof(val), sceNetInetGetsockopt(client, INET_SOL_SOCKET, INET_SO_ERROR, &val, &len), );
	closePair(client, server);

	// Each on a new connection.
	TIME_N(TCP_REPEAT, "shutdown(SHUT_WR)",
		connectPair(listener, &la, &client, &server),
		sceNetInetShutdown(client, INET_SHUT_WR),
		closePair(client, server));
	TIME_N(TCP_REPEAT, "shutdown(SHUT_RD)",
		connectPair(listener, &la, &client, &server),
		sceNetInetShutdown(client, INET_SHUT_RD),
		closePair(client, server));
	TIME_N(TCP_REPEAT, "close(TCP, connected)",
		connectPair(listener, &la, &client, &server),
		sceNetInetClose(client),
		closePair(-1, server));
	TIME_N(TCP_REPEAT, "CloseWithRST(TCP, connected)",
		connectPair(listener, &la, &client, &server),
		sceNetInetCloseWithRST(client),
		closePair(-1, server));
	TIME_N(TCP_REPEAT, "connect(TCP, blocking, to a listener)",
		client = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0),
		sceNetInetConnect(client, &la, sizeof(la)),
		(server = inetWait(listener, 0, 500) > 0 ? sceNetInetAccept(listener, NULL, NULL) : -1, closePair(client, server)));
	InetAddr nobody = inetMakeAddr(LOOPBACK, PORT + 3);
	TIME_N(TCP_REPEAT, "connect(TCP, blocking, refused)",
		client = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0),
		sceNetInetConnect(client, &nobody, sizeof(nobody)),
		sceNetInetClose(client));
	TIME_N(TCP_REPEAT, "connect(TCP, non-blocking)",
		(client = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0), sceNetInetSetsockopt(client, INET_SOL_SOCKET, INET_SO_NBIO, &nb, sizeof(nb))),
		sceNetInetConnect(client, &la, sizeof(la)),
		(server = inetWait(listener, 0, 500) > 0 ? sceNetInetAccept(listener, NULL, NULL) : -1, closePair(client, server)));
	sceNetInetClose(listener);
}

static void timeStats() {
	static const int counts[] = { 1, 8, 32 };
	printf("\nPCB lists by number of UDP sockets:\n");
	static int socks[32];
	static u8 buf[0x4000];
	for (int c = 0; c < 3; ++c) {
		for (int i = 0; i < counts[c]; ++i) {
			socks[i] = udpSocket(PORT + 10 + i);
		}
		int size = 0;
		char title[64];
		snprintf(title, sizeof(title), "GetUdpcbstat size, %d sockets", counts[c]);
		TIME_EACH(title, size = 0, sceNetInetGetUdpcbstat(&size, NULL), );
		snprintf(title, sizeof(title), "GetUdpcbstat buffer, %d sockets", counts[c]);
		TIME_EACH(title, size = sizeof(buf), sceNetInetGetUdpcbstat(&size, buf), );
		snprintf(title, sizeof(title), "GetTcpcbstat size, %d UDP sockets", counts[c]);
		TIME_EACH(title, size = 0, sceNetInetGetTcpcbstat(&size, NULL), );
		for (int i = 0; i < counts[c]; ++i) {
			sceNetInetClose(socks[i]);
		}
	}
}

static void timeBad() {
	printf("\nOn a socket that isn't open:\n");
	char buf[16];
	int val = 0, len = 0;
	InetAddr addr = inetMakeAddr(LOOPBACK, PORT);
	TIME_EACH("recv", , sceNetInetRecv(200, buf, sizeof(buf), INET_MSG_DONTWAIT), );
	TIME_EACH("send", , sceNetInetSend(200, buf, sizeof(buf), 0), );
	TIME_EACH("sendto", , sceNetInetSendto(200, buf, sizeof(buf), 0, &addr, sizeof(addr)), );
	TIME_EACH("bind", , sceNetInetBind(200, &addr, sizeof(addr)), );
	TIME_EACH("close", , sceNetInetClose(200), );
	TIME_EACH("getsockopt", len = sizeof(val), sceNetInetGetsockopt(200, INET_SOL_SOCKET, INET_SO_TYPE, &val, &len), );
}

int main(int argc, char *argv[]) {
	if (!inetStartup()) {
		return 1;
	}
	timeConversions();
	timeSetup();
	timeOptions();
	timeTcp();
	timeStats();
	timeBad();
	inetShutdown();
	return 0;
}
