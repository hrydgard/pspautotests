// What works with no network: the sceNet utilities, sceNetApctl without connecting (handlers, scanning,
// a profile that doesn't exist), and the resolver with nowhere to send queries. The WLAN switch must be
// on. The scan finds whatever access points are around, so only whether it found any is printed.

#include "inet.h"

typedef struct {
	u32 pool;
	u32 maximum;
	u32 free;
} NetMallocStat;

void sceNetEtherNtostr(const u8 *mac, char *str);
void sceNetEtherStrton(const char *str, u8 *mac);
int sceNetGetMallocStat(NetMallocStat *stat);
int sceNetFreeThreadinfo(int thid);
int sceNetThreadAbort(int thid);
int sceWlanDevIsPowerOn(void);
int sceWlanGetSwitchState(void);

typedef void (*ApctlHandler)(int oldState, int newState, int event, int error, void *arg);
int sceNetApctlInit(int stacksize, int prio);
int sceNetApctlTerm(void);
int sceNetApctlConnect(int config);
int sceNetApctlDisconnect(void);
int sceNetApctlGetState(int *state);
int sceNetApctlGetInfo(int code, void *info);
int sceNetApctlAddHandler(ApctlHandler handler, void *arg);
int sceNetApctlDelHandler(int id);
int sceNetApctlScan(void);
int sceNetApctlScanUser(void);
int sceNetApctlGetBSSDescIDList(int *size, void *buf);
int sceNetApctlGetBSSDescIDListUser(int *size, void *buf);
int sceNetApctlGetBSSDescEntry(int id, int code, void *info);
int sceNetApctlGetBSSDescEntryUser(int id, int code, void *info);

int sceNetResolverInit(void);
int sceNetResolverTerm(void);
int sceNetResolverCreate(int *rid, void *buf, int len);
int sceNetResolverDelete(int rid);
int sceNetResolverStartNtoA(int rid, const char *hostname, u32 *addr, int timeout, int retry);
int sceNetResolverStartAtoN(int rid, const u32 *addr, char *hostname, int len, int timeout, int retry);
int sceNetResolverStartNtoAAsync(int rid, const char *hostname, u32 *addr, int timeout, int retry);
int sceNetResolverStartAtoNAsync(int rid, const u32 *addr, char *hostname, int len, int timeout, int retry);
int sceNetResolverWaitAsync(int rid, int *result);
int sceNetResolverPollAsync(int rid, int *result);
int sceNetResolverStop(int rid);

static const char *bucket(u64 us) {
	if (us < 100000) {
		return "under 0.1s";
	} else if (us < 1000000) {
		return "0.1-1s";
	} else if (us < 3000000) {
		return "1-3s";
	} else if (us < 10000000) {
		return "3-10s";
	}
	return "over 10s";
}

static void testEther() {
	printf("Ether strings:\n");
	u8 mac[6] = { 0x00, 0x1A, 0x2B, 0xC3, 0xD4, 0xFF };
	char str[32];
	memset(str, 'X', sizeof(str));
	sceNetEtherNtostr(mac, str);
	str[31] = 0;
	printf("  Ntostr: '%s'\n", str);
	static const char *inputs[] = { "01:23:45:67:89:ab", "01:23:45:67:89:AB", "1:2:3:4:5:6", "01-23-45-67-89-ab", "01:23", "zz:zz:zz:zz:zz:zz", "" };
	for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
		memset(mac, 0xEE, sizeof(mac));
		sceNetEtherStrton(inputs[i], mac);
		printf("  Strton '%s': %02x:%02x:%02x:%02x:%02x:%02x\n", inputs[i], mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	}

	printf("\nGetMallocStat:\n");
	NetMallocStat stat = { 0, 0, 0 };
	int result = sceNetGetMallocStat(&stat);
	printf("  %08x, pool %x, maximum %s, free %s\n", result, (int)stat.pool, stat.maximum > 0 && stat.maximum <= stat.pool ? "in range" : "odd", stat.free > 0 && stat.free <= stat.pool ? "in range" : "odd");
	printf("  NULL: %08x\n", sceNetGetMallocStat(NULL));

	printf("\nWLAN:\n");
	printf("  sceWlanDevIsPowerOn: %d\n", sceWlanDevIsPowerOn());
	printf("  sceWlanGetSwitchState: %d\n", sceWlanGetSwitchState());
}

static volatile int threadResult;
static volatile int threadErrno;
static volatile int threadDone;

static int recvThread(SceSize argc, void *argv) {
	u8 buf[32];
	threadResult = sceNetInetRecv(*(int *)argv, buf, sizeof(buf), 0);
	threadErrno = sceNetInetGetErrno();
	threadDone = 1;
	return 0;
}

static void testThreads() {
	printf("\nThreadAbort / FreeThreadinfo:\n");
	int s = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	InetAddr addr = inetMakeAddr(LOOPBACK, 24600);
	sceNetInetBind(s, &addr, sizeof(addr));
	u32 timeout = 2000000;
	sceNetInetSetsockopt(s, INET_SOL_SOCKET, INET_SO_RCVTIMEO, &timeout, sizeof(timeout));
	threadDone = 0;
	SceUID thread = sceKernelCreateThread("recv", &recvThread, 0x18, 0x4000, 0, NULL);
	sceKernelStartThread(thread, sizeof(s), &s);
	sceKernelDelayThread(100000);
	printf("  ThreadAbort on the blocked thread: %08x\n", sceNetThreadAbort(thread));
	for (int i = 0; i < 50 && !threadDone; ++i) {
		sceKernelDelayThread(10000);
	}
	printf("  Blocked recv: within 0.5s %d, result %d, errno %d\n", threadDone, threadResult, threadErrno);
	sceKernelWaitThreadEnd(thread, NULL);
	printf("  FreeThreadinfo on the ended thread: %08x\n", sceNetFreeThreadinfo(thread));
	sceKernelDeleteThread(thread);
	printf("  ThreadAbort on the current thread: %08x\n", sceNetThreadAbort(sceKernelGetThreadId()));
	printf("  ThreadAbort on 0: %08x\n", sceNetThreadAbort(0));
	printf("  ThreadAbort on a bad ID: %08x\n", sceNetThreadAbort(0x1234));
	printf("  FreeThreadinfo on the current thread: %08x\n", sceNetFreeThreadinfo(sceKernelGetThreadId()));
	printf("  FreeThreadinfo on 0: %08x\n", sceNetFreeThreadinfo(0));
	printf("  FreeThreadinfo on a bad ID: %08x\n", sceNetFreeThreadinfo(0x1234));
	sceNetInetClose(s);
}

#define MAX_EVENTS 32
static volatile int numEvents = 0;
static int events[MAX_EVENTS][4];

static void apctlHandler(int oldState, int newState, int event, int error, void *arg) {
	if (numEvents < MAX_EVENTS) {
		events[numEvents][0] = oldState;
		events[numEvents][1] = newState;
		events[numEvents][2] = event;
		events[numEvents][3] = error;
		numEvents++;
	}
}

static void otherHandler(int oldState, int newState, int event, int error, void *arg) {
}

static void printEvents(int from) {
	printf("    events:");
	if (numEvents == from) {
		printf(" none");
	}
	for (int i = from; i < numEvents; ++i) {
		printf(" %d->%d e%d(%08x)", events[i][0], events[i][1], events[i][2], events[i][3]);
	}
	printf("\n");
}

static int waitIdle(int timeoutMs) {
	int state = -1;
	for (int i = 0; i < timeoutMs / 10; ++i) {
		sceNetApctlGetState(&state);
		if (state == 0) {
			break;
		}
		sceKernelDelayThread(10000);
	}
	return state;
}

static void testScan(const char *title, int (*scan)(void), int (*list)(int *, void *), int (*entry)(int, int, void *)) {
	printf("  %s:\n", title);
	int from = numEvents;
	u64 start = sceKernelGetSystemTimeWide();
	int result = scan();
	int state = -1;
	sceNetApctlGetState(&state);
	printf("    Scan: %08x, state right after %d\n", result, state);
	printf("    Scan again while scanning: %08x\n", scan());
	state = waitIdle(15000);
	printf("    Back to state %d after %s\n", state, bucket(sceKernelGetSystemTimeWide() - start));
	printEvents(from);

	int size = 0x1234;
	result = list(&size, NULL);
	printf("    List size query: %08x, %s\n", result, size == 0 ? "none" : (size > 0 && size % 8 == 0 ? "some, a multiple of 8" : "odd"));
	static u8 ids[1024];
	memset(ids, 0xCC, sizeof(ids));
	int got = sizeof(ids);
	result = list(&got, ids);
	printf("    List: %08x, size %s\n", result, got == size ? "the same" : "different");
	if (got >= 8) {
		u32 next, id;
		memcpy(&next, ids, 4);
		memcpy(&id, ids + 4, 4);
		printf("    First entry: next %s, id %d\n", next == 0 ? "0" : ((u8 *)next == ids + 8 ? "the next entry" : "other"), (int)id);
		u8 info[128];
		static const int codes[] = { 0, 1, 2, 3, 4, 5, 6 };
		for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i) {
			memset(info, 0, sizeof(info));
			result = entry(id, codes[i], info);
			printf("    Entry code %d: %08x\n", codes[i], result);
		}
		printf("    Entry, bad id: %08x\n", entry(1000, 0, info));
		printf("    Entry, bad code: %08x\n", entry(id, 100, info));
	}
	printf("    List NULL size: %08x\n", list(NULL, NULL));
}

static void testApctl() {
	printf("\nApctl:\n");
	int state = -1;
	printf("  GetState before init: %08x\n", sceNetApctlGetState(&state));
	printf("  Init: %08x\n", sceNetApctlInit(0x1600, 0x30));
	printf("  Init again: %08x\n", sceNetApctlInit(0x1600, 0x30));
	state = -1;
	printf("  GetState: %08x, state %d\n", sceNetApctlGetState(&state), state);
	printf("  GetState NULL: %08x\n", sceNetApctlGetState(NULL));
	u8 info[256];
	static const int codes[] = { 0, 1, 2, 8, 10, 11, 20 };
	for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i) {
		printf("  GetInfo %d, disconnected: %08x\n", codes[i], sceNetApctlGetInfo(codes[i], info));
	}

	printf("\n  Handlers:\n");
	int first = sceNetApctlAddHandler(&apctlHandler, NULL);
	printf("    Add: %s\n", first >= 0 ? "id" : "error");
	int ids[8];
	int n = 0;
	for (; n < 8; ++n) {
		ids[n] = sceNetApctlAddHandler(&otherHandler, (void *)n);
		if (ids[n] < 0) {
			break;
		}
	}
	printf("    More handlers: %d added, then %08x\n", n, n < 8 ? ids[n] : 0);
	printf("    Same function again: %s\n", n > 0 ? "allowed above" : "refused");
	printf("    Add NULL: %08x\n", sceNetApctlAddHandler(NULL, NULL));
	if (n > 0) {
		printf("    Del: %08x\n", sceNetApctlDelHandler(ids[0]));
		printf("    Del again: %08x\n", sceNetApctlDelHandler(ids[0]));
		int again = sceNetApctlAddHandler(&otherHandler, NULL);
		printf("    Add after Del: %s\n", again >= 0 ? (again == ids[0] ? "id reused" : "new id") : "error");
		if (again >= 0) {
			sceNetApctlDelHandler(again);
		}
	}
	for (int i = 1; i < n; ++i) {
		sceNetApctlDelHandler(ids[i]);
	}
	printf("    Del bad id: %08x\n", sceNetApctlDelHandler(100));
	printf("    Del -1: %08x\n", sceNetApctlDelHandler(-1));

	printf("\n  Scanning:\n");
	testScan("Scan", sceNetApctlScan, sceNetApctlGetBSSDescIDList, sceNetApctlGetBSSDescEntry);
	testScan("ScanUser", sceNetApctlScanUser, sceNetApctlGetBSSDescIDListUser, sceNetApctlGetBSSDescEntryUser);

	// Profile 0 isn't tried: it returns 0 and goes on to JOINING and GETTING_IP, which leaves apctl busy.
	printf("\n  Connecting to profiles that don't exist:\n");
	static const int profiles[] = { 10, 11, -1, 100 };
	for (size_t i = 0; i < sizeof(profiles) / sizeof(profiles[0]); ++i) {
		int from = numEvents;
		int result = sceNetApctlConnect(profiles[i]);
		sceKernelDelayThread(1000000);
		state = waitIdle(5000);
		printf("    Profile %d: %08x, state %d\n", profiles[i], result, state);
		printEvents(from);
	}
	printf("  Disconnect while disconnected: %08x\n", sceNetApctlDisconnect());
	printf("  Del the first handler: %08x\n", sceNetApctlDelHandler(first));
	printf("  Term: %08x\n", sceNetApctlTerm());
	printf("  Term again: %08x\n", sceNetApctlTerm());
	printf("  GetState after term: %08x\n", sceNetApctlGetState(&state));
}

static volatile int lookupResult;
static volatile int lookupDone;
static int lookupRid;

static int lookupThread(SceSize argc, void *argv) {
	u32 addr = 0;
	lookupResult = sceNetResolverStartNtoA(lookupRid, "www.ppsspp.org", &addr, 5, 3);
	lookupDone = 1;
	return 0;
}

static void testResolver() {
	printf("\nResolver:\n");
	int rid = -1;
	static u8 rbuf[1024];
	printf("  Create before init: %08x\n", sceNetResolverCreate(&rid, rbuf, sizeof(rbuf)));
	u32 early = 0;
	printf("  NtoA before init: %08x\n", sceNetResolverStartNtoA(rid, "127.0.0.1", &early, 1, 1));
	printf("  Stop before init: %08x\n", sceNetResolverStop(rid));
	printf("  Delete before init: %08x\n", sceNetResolverDelete(rid));
	printf("  Init: %08x\n", sceNetResolverInit());
	printf("  Init again: %08x\n", sceNetResolverInit());
	printf("  Create NULL rid: %08x\n", sceNetResolverCreate(NULL, rbuf, sizeof(rbuf)));
	rid = -1;
	int result = sceNetResolverCreate(&rid, NULL, 0);
	printf("  Create NULL buffer: %08x, rid %s\n", result, rid >= 0 ? "set" : "not set");
	if (result >= 0) {
		sceNetResolverDelete(rid);
	}
	static const int sizes[] = { 0, 1, 127, 128, 512 };
	static u8 bufs[5][1024];
	for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
		rid = -1;
		result = sceNetResolverCreate(&rid, bufs[i], sizes[i]);
		printf("  Create, buffer %d: %08x\n", sizes[i], result);
		if (result >= 0) {
			sceNetResolverDelete(rid);
		}
	}
	int rids[20];
	int count = 0;
	for (; count < 20; ++count) {
		result = sceNetResolverCreate(&rids[count], bufs[count % 5], 1024);
		if (result < 0) {
			break;
		}
	}
	printf("  Resolvers at once: %d, then %08x\n", count, count < 20 ? result : 0);
	for (int i = 0; i < count; ++i) {
		sceNetResolverDelete(rids[i]);
	}
	printf("  Delete bad id: %08x\n", sceNetResolverDelete(100));

	result = sceNetResolverCreate(&rid, rbuf, sizeof(rbuf));
	printf("  Create: %08x\n", result);
	u32 addr = 0;
	static const char *names[] = { "127.0.0.1", "10.1.2.3", "localhost", "www.ppsspp.org", "" };
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
		addr = 0;
		u64 start = sceKernelGetSystemTimeWide();
		result = sceNetResolverStartNtoA(rid, names[i], &addr, 1, 1);
		printf("  NtoA '%s': %08x, address %08x, %s\n", names[i], result, (unsigned)addr, bucket(sceKernelGetSystemTimeWide() - start));
	}
	printf("  NtoA NULL name: %08x\n", sceNetResolverStartNtoA(rid, NULL, &addr, 1, 1));
	printf("  NtoA NULL address: %08x\n", sceNetResolverStartNtoA(rid, "127.0.0.1", NULL, 1, 1));
	printf("  NtoA timeout 0: %08x\n", sceNetResolverStartNtoA(rid, "127.0.0.1", &addr, 0, 1));
	printf("  NtoA retry 0: %08x\n", sceNetResolverStartNtoA(rid, "127.0.0.1", &addr, 1, 0));
	printf("  NtoA bad rid: %08x\n", sceNetResolverStartNtoA(100, "127.0.0.1", &addr, 1, 1));

	char host[64];
	u32 ip = inetHtonl(LOOPBACK);
	memset(host, 0, sizeof(host));
	u64 start = sceKernelGetSystemTimeWide();
	result = sceNetResolverStartAtoN(rid, &ip, host, sizeof(host), 1, 1);
	printf("  AtoN 127.0.0.1: %08x, '%s', %s\n", result, host, bucket(sceKernelGetSystemTimeWide() - start));
	memset(host, 0, sizeof(host));
	result = sceNetResolverStartAtoN(rid, &ip, host, 0, 1, 1);
	printf("  AtoN, length 0: %08x\n", result);

	printf("\n  Async:\n");
	addr = 0;
	printf("    NtoAAsync 127.0.0.1: %08x\n", sceNetResolverStartNtoAAsync(rid, "127.0.0.1", &addr, 1, 1));
	int status = 0x1234;
	result = sceNetResolverPollAsync(rid, &status);
	printf("    PollAsync: %08x, status %08x\n", result, status);
	status = 0x1234;
	result = sceNetResolverWaitAsync(rid, &status);
	printf("    WaitAsync: %08x, status %08x, address %08x\n", result, status, (unsigned)addr);
	status = 0x1234;
	printf("    PollAsync after: %08x, status %08x\n", sceNetResolverPollAsync(rid, &status), status);
	printf("    WaitAsync with nothing pending: %08x\n", sceNetResolverWaitAsync(rid, &status));
	printf("    NtoAAsync www.ppsspp.org: %08x\n", sceNetResolverStartNtoAAsync(rid, "www.ppsspp.org", &addr, 1, 1));
	printf("    NtoAAsync while one is pending: %08x\n", sceNetResolverStartNtoAAsync(rid, "127.0.0.1", &addr, 1, 1));
	status = 0x1234;
	start = sceKernelGetSystemTimeWide();
	result = sceNetResolverWaitAsync(rid, &status);
	printf("    WaitAsync: %08x, status %08x, %s\n", result, status, bucket(sceKernelGetSystemTimeWide() - start));
	memset(host, 0, sizeof(host));
	printf("    AtoNAsync 127.0.0.1: %08x\n", sceNetResolverStartAtoNAsync(rid, &ip, host, sizeof(host), 1, 1));
	status = 0x1234;
	result = sceNetResolverWaitAsync(rid, &status);
	printf("    WaitAsync: %08x, status %08x, '%s'\n", result, status, host);
	printf("    PollAsync bad rid: %08x\n", sceNetResolverPollAsync(100, &status));
	printf("    PollAsync NULL: %08x\n", sceNetResolverPollAsync(rid, NULL));

	printf("\n  Stop:\n");
	printf("    Stop with nothing running: %08x\n", sceNetResolverStop(rid));
	printf("    Stop bad rid: %08x\n", sceNetResolverStop(100));
	lookupRid = rid;
	lookupDone = 0;
	lookupResult = 0x1234;
	SceUID thread = sceKernelCreateThread("lookup", &lookupThread, 0x18, 0x4000, 0, NULL);
	sceKernelStartThread(thread, 0, NULL);
	sceKernelDelayThread(100000);
	int before = lookupDone;
	printf("    Stop during a blocking lookup: %08x\n", sceNetResolverStop(rid));
	for (int i = 0; i < 50 && !lookupDone; ++i) {
		sceKernelDelayThread(10000);
	}
	printf("    Lookup: done before %d, within 0.5s %d, result %08x\n", before, lookupDone, lookupResult);
	sceKernelWaitThreadEnd(thread, NULL);
	sceKernelDeleteThread(thread);

	printf("  Delete: %08x\n", sceNetResolverDelete(rid));
	printf("  Delete again: %08x\n", sceNetResolverDelete(rid));
	printf("  Term: %08x\n", sceNetResolverTerm());
	printf("  Term again: %08x\n", sceNetResolverTerm());
	printf("  Create after term: %08x\n", sceNetResolverCreate(&rid, rbuf, sizeof(rbuf)));
}

int main(int argc, char *argv[]) {
	if (!inetStartup()) {
		return 1;
	}
	testEther();
	testThreads();
	testApctl();
	testResolver();
	inetShutdown();
	return 0;
}
