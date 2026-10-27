// sceNetInetGetsockopt and sceNetInetSetsockopt: the defaults of the common options on new TCP and
// UDP sockets, what setting them reads back as, the size checks, and the PSP's own SO_NBIO.

#include "inet.h"

static void printOpt(const char *title, int s, int level, int name) {
	u8 val[16];
	memset(val, 0xCC, sizeof(val));
	int len = sizeof(val);
	int result = sceNetInetGetsockopt(s, level, name, val, &len);
	if (result < 0) {
		printf("%s: %d, errno %d\n", title, result, sceNetInetGetErrno());
		return;
	}
	printf("%s: len %d,", title, len);
	for (int i = 0; i < len && i < 16; ++i) {
		printf(" %02x", val[i]);
	}
	printf("\n");
}

static void setInt(const char *title, int s, int level, int name, int value) {
	int result = sceNetInetSetsockopt(s, level, name, &value, sizeof(value));
	if (result < 0) {
		printf("%s = %d: %d, errno %d\n", title, value, result, sceNetInetGetErrno());
	} else {
		char t[64];
		snprintf(t, sizeof(t), "%s = %d, reads", title, value);
		printOpt(t, s, level, name);
	}
}

static const struct {
	const char *name;
	int level;
	int opt;
} options[] = {
	{ "SO_DEBUG", INET_SOL_SOCKET, INET_SO_DEBUG },
	{ "SO_ACCEPTCONN", INET_SOL_SOCKET, INET_SO_ACCEPTCONN },
	{ "SO_REUSEADDR", INET_SOL_SOCKET, INET_SO_REUSEADDR },
	{ "SO_KEEPALIVE", INET_SOL_SOCKET, INET_SO_KEEPALIVE },
	{ "SO_DONTROUTE", INET_SOL_SOCKET, INET_SO_DONTROUTE },
	{ "SO_BROADCAST", INET_SOL_SOCKET, INET_SO_BROADCAST },
	{ "SO_LINGER", INET_SOL_SOCKET, INET_SO_LINGER },
	{ "SO_OOBINLINE", INET_SOL_SOCKET, INET_SO_OOBINLINE },
	{ "SO_REUSEPORT", INET_SOL_SOCKET, INET_SO_REUSEPORT },
	{ "SO_SNDBUF", INET_SOL_SOCKET, INET_SO_SNDBUF },
	{ "SO_RCVBUF", INET_SOL_SOCKET, INET_SO_RCVBUF },
	{ "SO_SNDLOWAT", INET_SOL_SOCKET, INET_SO_SNDLOWAT },
	{ "SO_RCVLOWAT", INET_SOL_SOCKET, INET_SO_RCVLOWAT },
	{ "SO_SNDTIMEO", INET_SOL_SOCKET, INET_SO_SNDTIMEO },
	{ "SO_RCVTIMEO", INET_SOL_SOCKET, INET_SO_RCVTIMEO },
	{ "SO_ERROR", INET_SOL_SOCKET, INET_SO_ERROR },
	{ "SO_TYPE", INET_SOL_SOCKET, INET_SO_TYPE },
	{ "SO_NBIO", INET_SOL_SOCKET, INET_SO_NBIO },
	{ "IP_TTL", INET_IPPROTO_IP, INET_IP_TTL },
	{ "TCP_NODELAY", INET_IPPROTO_TCP, INET_TCP_NODELAY },
	{ "Unknown option 0x7777", INET_SOL_SOCKET, 0x7777 },
	{ "Unknown level 99", 99, INET_SO_TYPE },
};

int main(int argc, char *argv[]) {
	if (!inetStartup()) {
		return 1;
	}
	int tcp = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
	int udp = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);

	printf("Defaults, TCP:\n");
	for (int i = 0; i < (int)ARRAY_SIZE(options); ++i) {
		char t[64];
		snprintf(t, sizeof(t), "  %s", options[i].name);
		printOpt(t, tcp, options[i].level, options[i].opt);
	}
	printf("\nDefaults, UDP:\n");
	for (int i = 0; i < (int)ARRAY_SIZE(options); ++i) {
		char t[64];
		snprintf(t, sizeof(t), "  %s", options[i].name);
		printOpt(t, udp, options[i].level, options[i].opt);
	}

	printf("\nSetting, TCP:\n");
	setInt("  SO_REUSEADDR", tcp, INET_SOL_SOCKET, INET_SO_REUSEADDR, 1);
	setInt("  SO_REUSEADDR", tcp, INET_SOL_SOCKET, INET_SO_REUSEADDR, 5);
	setInt("  SO_KEEPALIVE", tcp, INET_SOL_SOCKET, INET_SO_KEEPALIVE, 1);
	setInt("  SO_SNDBUF", tcp, INET_SOL_SOCKET, INET_SO_SNDBUF, 1000);
	setInt("  SO_SNDBUF", tcp, INET_SOL_SOCKET, INET_SO_SNDBUF, 0x10000);
	setInt("  SO_SNDBUF", tcp, INET_SOL_SOCKET, INET_SO_SNDBUF, 0x100000);
	setInt("  SO_SNDBUF", tcp, INET_SOL_SOCKET, INET_SO_SNDBUF, 0);
	setInt("  SO_SNDBUF", tcp, INET_SOL_SOCKET, INET_SO_SNDBUF, -1);
	setInt("  SO_RCVBUF", tcp, INET_SOL_SOCKET, INET_SO_RCVBUF, 1000);
	setInt("  SO_RCVBUF", tcp, INET_SOL_SOCKET, INET_SO_RCVBUF, 0x100000);
	setInt("  SO_NBIO", tcp, INET_SOL_SOCKET, INET_SO_NBIO, 1);
	setInt("  SO_NBIO", tcp, INET_SOL_SOCKET, INET_SO_NBIO, 0);
	setInt("  TCP_NODELAY", tcp, INET_IPPROTO_TCP, INET_TCP_NODELAY, 1);
	setInt("  IP_TTL", tcp, INET_IPPROTO_IP, INET_IP_TTL, 10);
	setInt("  SO_TYPE", tcp, INET_SOL_SOCKET, INET_SO_TYPE, 2);
	setInt("  SO_ERROR", tcp, INET_SOL_SOCKET, INET_SO_ERROR, 1);
	setInt("  Unknown option 0x7777", tcp, INET_SOL_SOCKET, 0x7777, 1);

	printf("\nTimeouts (timeval):\n");
	InetTimeval tv = { 1, 500000 };
	printf("  Set SO_RCVTIMEO 1.5s: %d\n", sceNetInetSetsockopt(tcp, INET_SOL_SOCKET, INET_SO_RCVTIMEO, &tv, sizeof(tv)));
	printOpt("  Reads", tcp, INET_SOL_SOCKET, INET_SO_RCVTIMEO);
	u32 us = 250000;
	printf("  Set SO_RCVTIMEO as a 4-byte 250000: %d", sceNetInetSetsockopt(tcp, INET_SOL_SOCKET, INET_SO_RCVTIMEO, &us, sizeof(us)));
	printf(", errno %d\n", sceNetInetGetErrno());
	printOpt("  Reads", tcp, INET_SOL_SOCKET, INET_SO_RCVTIMEO);
	InetTimeval bad = { 0, 2000000 };
	printf("  Set SO_SNDTIMEO with usec 2000000: %d", sceNetInetSetsockopt(tcp, INET_SOL_SOCKET, INET_SO_SNDTIMEO, &bad, sizeof(bad)));
	printf(", errno %d\n", sceNetInetGetErrno());
	printOpt("  Reads", tcp, INET_SOL_SOCKET, INET_SO_SNDTIMEO);

	printf("\nLinger:\n");
	int linger[2] = { 1, 5 };
	printf("  Set SO_LINGER {1, 5}: %d\n", sceNetInetSetsockopt(tcp, INET_SOL_SOCKET, INET_SO_LINGER, linger, sizeof(linger)));
	printOpt("  Reads", tcp, INET_SOL_SOCKET, INET_SO_LINGER);

	printf("\nSizes:\n");
	int one = 1;
	inetPrint("  setsockopt len 0", sceNetInetSetsockopt(udp, INET_SOL_SOCKET, INET_SO_BROADCAST, &one, 0));
	inetPrint("  setsockopt len 1", sceNetInetSetsockopt(udp, INET_SOL_SOCKET, INET_SO_BROADCAST, &one, 1));
	inetPrint("  setsockopt len 8", sceNetInetSetsockopt(udp, INET_SOL_SOCKET, INET_SO_BROADCAST, &one, 8));
	int val = 0x12345678;
	int len = 2;
	int result = sceNetInetGetsockopt(udp, INET_SOL_SOCKET, INET_SO_TYPE, &val, &len);
	printf("  getsockopt len 2: %d, len %d, value %08x\n", result, len, val);
	len = 0;
	inetPrint("  getsockopt len 0", sceNetInetGetsockopt(udp, INET_SOL_SOCKET, INET_SO_TYPE, &val, &len));
	// NULL pointers aren't tried: a NULL address hangs bind, so these likely do too.
	inetPrint("  getsockopt bad socket", sceNetInetGetsockopt(200, INET_SOL_SOCKET, INET_SO_TYPE, &val, &len));

	printf("\nAfter listen:\n");
	InetAddr addr = inetMakeAddr(LOOPBACK, 24200);
	int listener = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
	sceNetInetBind(listener, &addr, sizeof(addr));
	sceNetInetListen(listener, 1);
	printOpt("  SO_ACCEPTCONN", listener, INET_SOL_SOCKET, INET_SO_ACCEPTCONN);

	sceNetInetClose(listener);
	sceNetInetClose(tcp);
	sceNetInetClose(udp);
	inetShutdown();
	return 0;
}
