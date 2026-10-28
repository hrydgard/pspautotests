// Timing of sceNetInet and the access point connection: how long startup and the connection take, what
// the cheap calls cost, round trips over loopback, and how accurate the timeouts are. A measurement,
// not a regression test: the numbers differ from run to run. Connects with profile 1, as pair/ does.

#include "inet.h"
#include <pspthreadman.h>

int sceNetApctlInit(int stacksize, int prio);
int sceNetApctlTerm(void);
int sceNetApctlConnect(int config);
int sceNetApctlDisconnect(void);
int sceNetApctlGetState(int *state);
int sceNetApctlGetInfo(int code, void *info);
int sceNetApctlAddHandler(void (*handler)(int oldState, int newState, int event, int error, void *arg), void *arg);
int sceNetResolverInit(void);
int sceNetResolverTerm(void);
int sceNetResolverCreate(int *rid, void *buf, int len);
int sceNetResolverDelete(int rid);
int sceNetResolverStartNtoA(int rid, const char *hostname, u32 *addr, int timeout, int retry);

#define PORT 24800
#define MAX_SAMPLES 200

static u64 timeNow() {
	return sceKernelGetSystemTimeWide();
}

typedef struct {
	int count;
	u32 us[MAX_SAMPLES];
} Samples;

static Samples s;

static void sampleAdd(Samples *set, u64 us) {
	if (set->count < MAX_SAMPLES) {
		set->us[set->count++] = us > 0xFFFFFFFF ? 0xFFFFFFFF : (u32)us;
	}
}

static void samplePrint(const char *title, Samples *set) {
	if (set->count == 0) {
		printf("  %-40s no samples\n", title);
		return;
	}
	for (int i = 1; i < set->count; ++i) {
		u32 v = set->us[i];
		int j = i - 1;
		while (j >= 0 && set->us[j] > v) {
			set->us[j + 1] = set->us[j];
			--j;
		}
		set->us[j + 1] = v;
	}
	u64 sum = 0;
	for (int i = 0; i < set->count; ++i) {
		sum += set->us[i];
	}
	printf("  %-40s n=%3d min %8u med %8u avg %8u max %8u us\n", title, set->count, (unsigned)set->us[0], (unsigned)set->us[set->count / 2], (unsigned)(sum / set->count), (unsigned)set->us[set->count - 1]);
	set->count = 0;
}

static void printOnce(const char *title, u64 us) {
	printf("  %-40s %8u us\n", title, (unsigned)us);
}

#define TIME_CALLS(title, n, call) do { \
	u64 _start = timeNow(); \
	for (int _i = 0; _i < (n); ++_i) { call; } \
	u64 _end = timeNow(); \
	printf("  %-40s %8u.%02u us each (%d calls)\n", title, (unsigned)((_end - _start) / (n)), (unsigned)(((_end - _start) * 100 / (n)) % 100), (n)); \
} while (0)

typedef struct {
	int oldState, newState, event, error;
	u64 time;
} ApctlEvent;

static volatile ApctlEvent events[32];
static volatile int eventCount = 0;

static void apctlHandler(int oldState, int newState, int event, int error, void *arg) {
	if (eventCount < 32) {
		events[eventCount].oldState = oldState;
		events[eventCount].newState = newState;
		events[eventCount].event = event;
		events[eventCount].error = error;
		events[eventCount].time = timeNow();
		eventCount++;
	}
}

// Polls the apctl state every 100 us until it's `state`. Returns when it got there, or 0.
static u64 waitApctlState(int state, int timeoutMs) {
	u64 end = timeNow() + (u64)timeoutMs * 1000;
	while (timeNow() < end) {
		int st = -1;
		sceNetApctlGetState(&st);
		if (st == state) {
			return timeNow();
		}
		sceKernelDelayThread(100);
	}
	return 0;
}

static void printEvents(int from, u64 start) {
	for (int i = from; i < eventCount; ++i) {
		printf("    +%8u us: state %d -> %d, event %d, error %08x\n", (unsigned)(events[i].time - start), events[i].oldState, events[i].newState, events[i].event, events[i].error);
	}
}

static int udpSocket(u16 port) {
	int sock = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	InetAddr addr = inetMakeAddr(LOOPBACK, port);
	sceNetInetBind(sock, &addr, sizeof(addr));
	return sock;
}

static void timeStartup() {
	printf("Startup:\n");
	u64 t = timeNow();
	sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	printOnce("LoadNetModule(COMMON)", timeNow() - t);
	t = timeNow();
	sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
	printOnce("LoadNetModule(INET)", timeNow() - t);
	t = timeNow();
	sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000);
	printOnce("sceNetInit", timeNow() - t);
	t = timeNow();
	sceNetInetInit();
	printOnce("sceNetInetInit", timeNow() - t);
	t = timeNow();
	sceNetApctlInit(0x1600, 0x30);
	printOnce("sceNetApctlInit", timeNow() - t);
	sceNetApctlAddHandler(&apctlHandler, NULL);
	t = timeNow();
	sceNetResolverInit();
	printOnce("sceNetResolverInit", timeNow() - t);
}

static int timeConnect(int round) {
	int from = eventCount;
	u64 t = timeNow();
	int result = sceNetApctlConnect(1);
	u64 returned = timeNow() - t;
	u64 at = waitApctlState(4, 20000);
	if (round == 0) {
		printf("  sceNetApctlConnect(1): %08x, returns in %u us\n", result, (unsigned)returned);
		printEvents(from, t);
	}
	if (at) {
		sampleAdd(&s, at - t);
	}
	return at != 0;
}

static void timeApctl() {
	printf("\nAccess point (profile 1):\n");
	int connected = 0;
	for (int round = 0; round < 3; ++round) {
		connected = timeConnect(round);
		if (!connected) {
			printf("  Didn't get an IP address within 20 s\n");
			break;
		}
		if (round < 2) {
			u64 t = timeNow();
			sceNetApctlDisconnect();
			u64 at = waitApctlState(0, 5000);
			if (round == 0) {
				printOnce("Disconnect -> state 0", at ? at - t : 0);
			}
			sceKernelDelayThread(200000);
		}
	}
	samplePrint("Connect -> state 4 (got IP)", &s);
	if (!connected) {
		return;
	}
	char info[256];
	TIME_CALLS("sceNetApctlGetState", 1000, { int st; sceNetApctlGetState(&st); });
	TIME_CALLS("sceNetApctlGetInfo(IP)", 100, sceNetApctlGetInfo(8, info));

	char rbuf[1024];
	int rid = -1;
	sceNetResolverCreate(&rid, rbuf, sizeof(rbuf));
	for (int i = 0; i < 3; ++i) {
		u32 addr = 0;
		u64 t = timeNow();
		int result = sceNetResolverStartNtoA(rid, "www.ppsspp.org", &addr, 2, 1);
		u64 took = timeNow() - t;
		if (i == 0) {
			printf("  Resolver NtoA www.ppsspp.org: %08x\n", result);
		}
		if (result >= 0) {
			sampleAdd(&s, took);
		}
	}
	samplePrint("Resolver NtoA (each)", &s);
	sceNetResolverDelete(rid);
}

static void timeCalls() {
	printf("\nCheap calls:\n");
	int udp = udpSocket(PORT);
	int val = 0, len = sizeof(val);
	TIME_CALLS("sceNetInetGetErrno", 1000, sceNetInetGetErrno());
	TIME_CALLS("sceNetInetGetsockopt(SO_TYPE)", 1000, { len = sizeof(val); sceNetInetGetsockopt(udp, INET_SOL_SOCKET, 0x1008, &val, &len); });
	TIME_CALLS("sceNetInetSetsockopt(SO_RCVBUF)", 1000, { val = 32768; sceNetInetSetsockopt(udp, INET_SOL_SOCKET, INET_SO_RCVBUF, &val, sizeof(val)); });
	TIME_CALLS("sceNetInetInetAddr", 1000, sceNetInetInetAddr("192.168.1.20"));
	InetAddr addr;
	TIME_CALLS("sceNetInetGetsockname", 1000, { len = sizeof(addr); sceNetInetGetsockname(udp, &addr, &len); });
	char buf[64];
	TIME_CALLS("sceNetInetRecv, nothing there", 1000, sceNetInetRecv(udp, buf, sizeof(buf), INET_MSG_DONTWAIT));
	InetPollFd fd = { udp, 1, 0 };
	TIME_CALLS("sceNetInetPoll, timeout 0", 1000, sceNetInetPoll(&fd, 1, 0));
	InetFdSet set;
	InetTimeval tv0 = { 0, 0 };
	TIME_CALLS("sceNetInetSelect, timeout 0", 1000, { memset(&set, 0, sizeof(set)); set.bits[udp >> 5] |= 1U << (udp & 31); sceNetInetSelect(udp + 1, &set, NULL, NULL, &tv0); });
	sceNetInetClose(udp);

	for (int i = 0; i < 50; ++i) {
		u64 t = timeNow();
		int sock = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
		sampleAdd(&s, timeNow() - t);
		sceNetInetClose(sock);
	}
	samplePrint("socket(UDP)", &s);
	for (int i = 0; i < 50; ++i) {
		int sock = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
		u64 t = timeNow();
		sceNetInetClose(sock);
		sampleAdd(&s, timeNow() - t);
	}
	samplePrint("close(TCP, never used)", &s);
	for (int i = 0; i < 50; ++i) {
		int sock = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
		InetAddr a = inetMakeAddr(LOOPBACK, PORT + 1);
		u64 t = timeNow();
		sceNetInetBind(sock, &a, sizeof(a));
		sampleAdd(&s, timeNow() - t);
		sceNetInetClose(sock);
	}
	samplePrint("bind(UDP)", &s);
}

static void timeLoopback() {
	printf("\nLoopback:\n");
	int a = udpSocket(PORT + 2);
	int b = udpSocket(PORT + 3);
	InetAddr addrB = inetMakeAddr(LOOPBACK, PORT + 3);
	char buf[1024];
	Samples send = { 0 };
	for (int i = 0; i < 50; ++i) {
		u64 t = timeNow();
		sceNetInetSendto(a, buf, 64, 0, &addrB, sizeof(addrB));
		sampleAdd(&send, timeNow() - t);
		sceNetInetRecv(b, buf, sizeof(buf), 0);
		sampleAdd(&s, timeNow() - t);
	}
	samplePrint("UDP sendto returns (64 bytes)", &send);
	samplePrint("UDP sendto -> blocking recv done", &s);
	for (int i = 0; i < 50; ++i) {
		u64 t = timeNow();
		sceNetInetSendto(a, buf, 1024, 0, &addrB, sizeof(addrB));
		sceNetInetRecv(b, buf, sizeof(buf), 0);
		sampleAdd(&s, timeNow() - t);
	}
	samplePrint("UDP round, 1024 bytes", &s);
	sceNetInetClose(a);
	sceNetInetClose(b);

	int listener = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
	InetAddr la = inetMakeAddr(LOOPBACK, PORT + 4);
	sceNetInetBind(listener, &la, sizeof(la));
	sceNetInetListen(listener, 4);
	Samples acc = { 0 };
	int client = -1, server = -1;
	for (int i = 0; i < 20; ++i) {
		client = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
		u64 t = timeNow();
		sceNetInetConnect(client, &la, sizeof(la));
		sampleAdd(&s, timeNow() - t);
		t = timeNow();
		server = sceNetInetAccept(listener, NULL, NULL);
		sampleAdd(&acc, timeNow() - t);
		if (i < 19) {
			sceNetInetClose(client);
			sceNetInetClose(server);
		}
	}
	samplePrint("TCP connect (blocking)", &s);
	samplePrint("accept, connection waiting", &acc);
	for (int i = 0; i < 50; ++i) {
		u64 t = timeNow();
		sceNetInetSend(client, buf, 1, 0);
		sceNetInetRecv(server, buf, 1, 0);
		sceNetInetSend(server, buf, 1, 0);
		sceNetInetRecv(client, buf, 1, 0);
		sampleAdd(&s, timeNow() - t);
	}
	samplePrint("TCP ping-pong, 1 byte each way", &s);
	sceNetInetClose(client);
	sceNetInetClose(server);
	sceNetInetClose(listener);
}

static void timeTimeouts() {
	printf("\nTimeouts (nothing arrives):\n");
	int udp = udpSocket(PORT + 5);
	char buf[16];
	static const int ms[] = { 10, 50 };
	for (int m = 0; m < 2; ++m) {
		char title[64];
		for (int i = 0; i < 10; ++i) {
			InetPollFd fd = { udp, 1, 0 };
			u64 t = timeNow();
			sceNetInetPoll(&fd, 1, ms[m]);
			sampleAdd(&s, timeNow() - t);
		}
		snprintf(title, sizeof(title), "poll, %d ms", ms[m]);
		samplePrint(title, &s);
		for (int i = 0; i < 10; ++i) {
			InetFdSet set;
			memset(&set, 0, sizeof(set));
			set.bits[udp >> 5] |= 1U << (udp & 31);
			InetTimeval tv = { 0, ms[m] * 1000 };
			u64 t = timeNow();
			sceNetInetSelect(udp + 1, &set, NULL, NULL, &tv);
			sampleAdd(&s, timeNow() - t);
		}
		snprintf(title, sizeof(title), "select, %d ms", ms[m]);
		samplePrint(title, &s);
		int us = ms[m] * 1000;
		sceNetInetSetsockopt(udp, INET_SOL_SOCKET, INET_SO_RCVTIMEO, &us, sizeof(us));
		for (int i = 0; i < 10; ++i) {
			u64 t = timeNow();
			sceNetInetRecv(udp, buf, sizeof(buf), 0);
			sampleAdd(&s, timeNow() - t);
		}
		snprintf(title, sizeof(title), "recv, SO_RCVTIMEO %d ms", ms[m]);
		samplePrint(title, &s);
	}
	sceNetInetClose(udp);
}

static void timeShutdown() {
	printf("\nShutdown:\n");
	u64 t = timeNow();
	sceNetApctlDisconnect();
	waitApctlState(0, 5000);
	printOnce("sceNetApctlDisconnect -> state 0", timeNow() - t);
	t = timeNow();
	sceNetResolverTerm();
	printOnce("sceNetResolverTerm", timeNow() - t);
	t = timeNow();
	sceNetApctlTerm();
	printOnce("sceNetApctlTerm", timeNow() - t);
	t = timeNow();
	sceNetInetTerm();
	printOnce("sceNetInetTerm", timeNow() - t);
	t = timeNow();
	sceNetTerm();
	printOnce("sceNetTerm", timeNow() - t);
}

int main(int argc, char *argv[]) {
	setvbuf(stdout, NULL, _IONBF, 0);
	timeStartup();
	timeCalls();
	timeLoopback();
	timeTimeouts();
	timeApctl();
	timeShutdown();
	sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
	return 0;
}
