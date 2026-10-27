// Two consoles on the same access point (profile 1, no security): what sceNetApctlGetInfo reports,
// finding each other with a UDP broadcast, UDP and TCP between them, and the resolver. Built as
// lan_host and lan_client; record with ../../adhoc/pair/record.py net/inet/pair/lan.

#include "../inet.h"

int sceNetApctlInit(int stacksize, int prio);
int sceNetApctlTerm(void);
int sceNetApctlConnect(int config);
int sceNetApctlDisconnect(void);
int sceNetApctlGetState(int *state);
int sceNetApctlGetInfo(int code, void *info);
int sceNetApctlAddHandler(void (*handler)(int oldState, int newState, int event, int error, void *arg), void *arg);

static volatile int apctlError = 0;
static volatile int apctlErrorEvent = -1;

static void apctlHandler(int oldState, int newState, int event, int error, void *arg) {
	if (error != 0 && apctlError == 0) {
		apctlError = error;
		apctlErrorEvent = event;
	}
}
int sceNetResolverInit(void);
int sceNetResolverTerm(void);
int sceNetResolverCreate(int *rid, void *buf, int len);
int sceNetResolverDelete(int rid);
int sceNetResolverStartNtoA(int rid, const char *hostname, u32 *addr, int timeout, int retry);
int sceNetResolverStartAtoN(int rid, const u32 *addr, char *hostname, int len, int timeout, int retry);

#define DISCOVER_HOST_PORT 25000
#define DISCOVER_CLIENT_PORT 25001
#define SYNC_PORT 25003
#define TCP_PORT 25004
#define UDP_PORT 25005

static u32 localIp = 0;
static u32 peerIp = 0;
static int syncSock = -1;
static u8 buf[0x8000];

static u32 parseIp(const char *s) {
	u32 a = 0;
	sceNetInetInetAton(s, &a);
	return inetHtonl(a);
}

static const char *ipKind(u32 ip) {
	if ((ip >> 24) == 10 || (ip >> 20) == 0xAC1 || (ip >> 16) == 0xC0A8)
		return "private";
	if (ip == 0)
		return "0.0.0.0";
	return "other";
}

static int connectAp() {
	printf("sceNetApctlInit: %08x\n", sceNetApctlInit(0x1600, 0x30));
	sceNetApctlAddHandler(&apctlHandler, NULL);
	printf("sceNetApctlConnect: %08x\n", sceNetApctlConnect(1));
	int state = 0;
	for (int i = 0; i < 300; ++i) {
		sceNetApctlGetState(&state);
		if (state == 4) {
			break;
		}
		sceKernelDelayThread(100000);
	}
	printf("apctl state: %d\n", state);
	if (state != 4) {
		printf("  Error event %d: %08x\n", apctlErrorEvent, apctlError);
		return 0;
	}

	union {
		char str[128];
		u8 bytes[128];
		u32 word;
	} info;
	printf("\nApctl info:\n");
	memset(&info, 0, sizeof(info));
	sceNetApctlGetInfo(0, &info);
	printf("  Profile name: %s\n", info.str[0] ? "set" : "empty");
	memset(&info, 0, sizeof(info));
	sceNetApctlGetInfo(2, &info);
	int ssidLen = (int)strlen(info.str);
	memset(&info, 0, sizeof(info));
	sceNetApctlGetInfo(3, &info);
	printf("  SSID: length matches SSID_LENGTH: %s\n", (int)info.word == ssidLen ? "yes" : "no");
	sceNetApctlGetInfo(4, &info);
	printf("  Security type: %d\n", (int)info.word);
	memset(&info, 0, sizeof(info));
	sceNetApctlGetInfo(5, &info);
	printf("  Strength: %s\n", info.bytes[0] > 0 && info.bytes[0] <= 100 ? "1-100" : "out of range");
	memset(&info, 0, sizeof(info));
	sceNetApctlGetInfo(6, &info);
	printf("  Channel: %s\n", info.bytes[0] >= 1 && info.bytes[0] <= 14 ? "1-14" : "out of range");
	memset(&info, 0, sizeof(info));
	sceNetApctlGetInfo(8, &info);
	localIp = parseIp(info.str);
	printf("  IP: %s\n", ipKind(localIp));
	memset(&info, 0, sizeof(info));
	sceNetApctlGetInfo(9, &info);
	printf("  Subnet mask: %s\n", info.str[0] ? "set" : "empty");
	memset(&info, 0, sizeof(info));
	sceNetApctlGetInfo(10, &info);
	printf("  Gateway: %s\n", ipKind(parseIp(info.str)));
	memset(&info, 0, sizeof(info));
	sceNetApctlGetInfo(11, &info);
	printf("  Primary DNS: %s\n", info.str[0] ? "set" : "empty");
	int result = sceNetApctlGetInfo(19, &info);
	printf("  Code 19: %08x\n", result);
	return 1;
}

// Finds the other console with a broadcast. The host broadcasts until the client answers.
static int discoverPeer() {
	printf("\nDiscovery:\n");
	int s = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	InetAddr me = inetMakeAddr(0, HOST ? DISCOVER_HOST_PORT : DISCOVER_CLIENT_PORT);
	sceNetInetBind(s, &me, sizeof(me));
	int one = 1;
	sceNetInetSetsockopt(s, INET_SOL_SOCKET, INET_SO_BROADCAST, &one, sizeof(one));
	int found = 0;
#if HOST
	InetAddr bcast = inetMakeAddr(0xFFFFFFFF, DISCOVER_CLIENT_PORT);
	int sendResult = 0;
	for (int i = 0; i < 300 && !found; ++i) {
		sendResult = sceNetInetSendto(s, "HOST", 4, 0, &bcast, sizeof(bcast));
		if (inetWait(s, 0, 100) > 0) {
			InetAddr from;
			int fromlen = sizeof(from);
			if (sceNetInetRecvfrom(s, buf, 16, 0, &from, &fromlen) == 4 && memcmp(buf, "CLNT", 4) == 0) {
				peerIp = inetHtonl(from.addr);
				found = 1;
			}
		}
	}
	printf("  Broadcast to 255.255.255.255: %d\n", sendResult);
	printf("  Client answered: %s\n", found ? "yes" : "no");
#else
	for (int i = 0; i < 300 && !found; ++i) {
		if (inetWait(s, 0, 100) > 0) {
			InetAddr from;
			int fromlen = sizeof(from);
			if (sceNetInetRecvfrom(s, buf, 16, 0, &from, &fromlen) == 4 && memcmp(buf, "HOST", 4) == 0) {
				peerIp = inetHtonl(from.addr);
				found = 1;
				// Keep answering for a while: the first unicast to a new address waits for ARP.
				InetAddr to = inetMakeAddr(peerIp, DISCOVER_HOST_PORT);
				int sentOk = 0, lastError = 0;
				for (int j = 0; j < 30; ++j) {
					int r = sceNetInetSendto(s, "CLNT", 4, 0, &to, sizeof(to));
					if (r == 4) {
						sentOk++;
					} else {
						lastError = sceNetInetGetErrno();
					}
					sceKernelDelayThread(100000);
				}
				printf("  Answers sent: %s, errno %d\n", sentOk == 30 ? "all 30" : (sentOk == 0 ? "none" : "some"), lastError);
			}
		}
	}
	printf("  Got the host's broadcast: %s\n", found ? "yes" : "no");
#endif
	printf("  Peer is on the same subnet: %s\n", (peerIp >> 8) == (localIp >> 8) ? "yes" : "no");
	sceNetInetClose(s);
	return found;
}

// Both sides call this with the same step, in the same order; it returns once the other side has
// reached it too, or after 10 seconds.
static int lanSync(int step) {
	if (syncSock < 0) {
		syncSock = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
		InetAddr me = inetMakeAddr(0, SYNC_PORT);
		sceNetInetBind(syncSock, &me, sizeof(me));
	}
	InetAddr to = inetMakeAddr(peerIp, SYNC_PORT);
	u8 msg[4] = { 'S', 'Y', 'N', (u8)step };
	int done = 0;
	for (int i = 0; i < 200 && !done; ++i) {
		sceNetInetSendto(syncSock, msg, 4, 0, &to, sizeof(to));
		while (inetWait(syncSock, 0, 50) > 0) {
			u8 in[8];
			if (sceNetInetRecvfrom(syncSock, in, sizeof(in), 0, NULL, NULL) == 4 && in[3] >= step) {
				done = 1;
			}
		}
	}
	for (int i = 0; i < 5; ++i) {
		sceNetInetSendto(syncSock, msg, 4, 0, &to, sizeof(to));
		sceKernelDelayThread(20000);
	}
	if (!done) {
		printf("(sync %d timed out)\n", step);
	}
	return done;
}

static inline int checkPattern(int len, u8 seed) {
	for (int i = 0; i < len; ++i) {
		if (buf[i] != (u8)(i * 3 + seed)) {
			return 0;
		}
	}
	return 1;
}

static inline void fillPattern(int len, u8 seed) {
	for (int i = 0; i < len; ++i) {
		buf[i] = (u8)(i * 3 + seed);
	}
}

static void testTcp() {
	printf("\nTCP:\n");
#if HOST
	int listener = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
	InetAddr me = inetMakeAddr(0, TCP_PORT);
	sceNetInetBind(listener, &me, sizeof(me));
	sceNetInetListen(listener, 1);
	lanSync(1);
	int ready = inetWait(listener, 0, 5000);
	InetAddr from;
	int fromlen = sizeof(from);
	int s = ready > 0 ? sceNetInetAccept(listener, &from, &fromlen) : -1;
	printf("  Accept: %s, from the client: %s\n", s >= 0 ? "ok" : "failed", inetHtonl(from.addr) == peerIp ? "yes" : "no");
	int total = 0;
	// Over a real access point, stalls of a few seconds happen.
	while (total < 20000 && inetWait(s, 0, 10000) > 0) {
		int r = sceNetInetRecv(s, buf + total, sizeof(buf) - total, 0);
		if (r <= 0) {
			break;
		}
		total += r;
	}
	printf("  Received %d, %s\n", total, checkPattern(total, 7) ? "intact" : "corrupt");
	inetPrint("  Send 100 back", sceNetInetSend(s, buf, 100, 0));
	lanSync(2);
	sceNetInetClose(s);
	sceNetInetClose(listener);
#else
	lanSync(1);
	int s = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
	InetAddr to = inetMakeAddr(peerIp, TCP_PORT);
	inetPrint("  Connect", sceNetInetConnect(s, &to, sizeof(to)));
	fillPattern(20000, 7);
	int sent = 0;
	while (sent < 20000) {
		int r = sceNetInetSend(s, buf + sent, 20000 - sent, 0);
		if (r <= 0) {
			break;
		}
		sent += r;
	}
	printf("  Sent %d\n", sent);
	int r = inetWait(s, 0, 10000) > 0 ? sceNetInetRecv(s, buf, sizeof(buf), 0) : -1;
	printf("  Received %d back\n", r);
	lanSync(2);
	sceNetInetClose(s);
	// A port nobody listens on.
	s = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
	to = inetMakeAddr(peerIp, TCP_PORT + 10);
	u64 start = sceKernelGetSystemTimeWide();
	int result = sceNetInetConnect(s, &to, sizeof(to));
	u64 took = sceKernelGetSystemTimeWide() - start;
	printf("  Connect to a closed port: %d, errno %d, %s\n", result, result < 0 ? sceNetInetGetErrno() : 0, took < 3000000 ? "within 3 seconds" : "slow");
	sceNetInetClose(s);
#endif
}

static void testUdp() {
	printf("\nUDP:\n");
	int s = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	InetAddr me = inetMakeAddr(0, UDP_PORT);
	sceNetInetBind(s, &me, sizeof(me));
	lanSync(3);
#if HOST
	int count = 0, inOrder = 1, last = -1;
	while (inetWait(s, 0, 1000) > 0) {
		int r = sceNetInetRecvfrom(s, buf, sizeof(buf), 0, NULL, NULL);
		if (r != 100) {
			continue;
		}
		if (buf[0] <= last) {
			inOrder = 0;
		}
		last = buf[0];
		count++;
	}
	printf("  50 datagrams: received %s, %s\n", count >= 45 ? "45 or more" : "fewer than 45", inOrder ? "in order" : "out of order");
#else
	InetAddr to = inetMakeAddr(peerIp, UDP_PORT);
	int sent = 0;
	for (int i = 0; i < 50; ++i) {
		buf[0] = (u8)i;
		if (sceNetInetSendto(s, buf, 100, 0, &to, sizeof(to)) == 100) {
			sent++;
		}
		sceKernelDelayThread(5000);
	}
	printf("  Sent %d datagrams\n", sent);
#endif
	lanSync(4);
	sceNetInetClose(s);
}

static inline void testResolver() {
	printf("\nResolver:\n");
	printf("  sceNetResolverInit: %08x\n", sceNetResolverInit());
	int rid = -1;
	static u8 rbuf[1024];
	printf("  Create: %08x\n", sceNetResolverCreate(&rid, rbuf, sizeof(rbuf)));
	u32 addr = 0;
	int result = sceNetResolverStartNtoA(rid, "www.ppsspp.org", &addr, 5, 2);
	printf("  NtoA www.ppsspp.org: %08x, address %s\n", result, addr != 0 ? "set" : "0");
	addr = 0;
	result = sceNetResolverStartNtoA(rid, "doesnotexist.invalid", &addr, 5, 2);
	printf("  NtoA doesnotexist.invalid: %08x, address %s\n", result, addr != 0 ? "set" : "0");
	addr = 0;
	result = sceNetResolverStartNtoA(rid, "127.0.0.1", &addr, 5, 2);
	printf("  NtoA 127.0.0.1: %08x, address %08lx\n", result, addr);
	printf("  Delete: %08x\n", sceNetResolverDelete(rid));
	printf("  Term: %08x\n", sceNetResolverTerm());
}

int main(int argc, char *argv[]) {
	if (!inetStartup()) {
		return 1;
	}
	printf("Role: %s\n", HOST ? "host" : "client");
	if (connectAp() && discoverPeer()) {
		testTcp();
		testUdp();
#if HOST
		testResolver();
#endif
	}
	if (syncSock >= 0) {
		sceNetInetClose(syncSock);
	}
	sceNetApctlDisconnect();
	sceKernelDelayThread(500000);
	sceNetApctlTerm();
	inetShutdown();
	return 0;
}
