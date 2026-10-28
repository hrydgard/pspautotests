// Timing of the ad hoc calls on one console, in a group of its own: how long calls take to return,
// how long until their events arrive, what the cheap calls cost, and how accurate the timeouts are.
// A measurement, not a regression test: the numbers differ from run to run.

#include "timing.h"

#define PORT 0x5000
#define ROUNDS 10

static Samples s;

static void timeStartup() {
	printf("Startup:\n");
	u64 t = timeNow();
	sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	printOnce("LoadNetModule(COMMON)", timeNow() - t);
	t = timeNow();
	sceUtilityLoadNetModule(PSP_NET_MODULE_ADHOC);
	printOnce("LoadNetModule(ADHOC)", timeNow() - t);
	t = timeNow();
	sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000);
	printOnce("sceNetInit", timeNow() - t);
	t = timeNow();
	sceNetAdhocInit();
	printOnce("sceNetAdhocInit", timeNow() - t);
	t = timeNow();
	sceNetAdhocctlInit(0x2000, 0x30, &TEST_ADHOC_ID);
	printOnce("sceNetAdhocctlInit", timeNow() - t);
	sceWlanGetEtherAddr(&localMac);
	t = timeNow();
	sceNetAdhocctlAddHandler(&timedHandler, NULL);
	printOnce("sceNetAdhocctlAddHandler", timeNow() - t);
}

static void timeBaseline() {
	printf("\nBaseline:\n");
	TIME_CALLS("sceKernelGetSystemTimeWide", 1000, timeNow());
	static const u32 delays[] = { 0, 50, 100, 1000, 10000 };
	for (int d = 0; d < 5; ++d) {
		for (int i = 0; i < 20; ++i) {
			u64 t = timeNow();
			sceKernelDelayThread(delays[d]);
			sampleAdd(&s, timeNow() - t);
		}
		char title[64];
		snprintf(title, sizeof(title), "sceKernelDelayThread(%u)", (unsigned)delays[d]);
		samplePrint(title, &s);
	}
}

static void timeGroup() {
	printf("\nCreate / Disconnect (%d rounds):\n", ROUNDS);
	Samples ret = { 0 }, ev = { 0 }, st = { 0 }, dret = { 0 }, dev = { 0 }, dst = { 0 };
	for (int r = 0; r < ROUNDS; ++r) {
		int from = timedEventCount;
		u64 t = timeNow();
		sceNetAdhocctlCreate("TIMING");
		sampleAdd(&ret, timeNow() - t);
		u64 stateAt = waitState(1, 5000);
		if (waitTimedEvent(from, 5000) >= 0) {
			sampleAdd(&ev, timedEvents[from].time - t);
		}
		if (stateAt) {
			sampleAdd(&st, stateAt - t);
		}
		sceKernelDelayThread(100000);

		from = timedEventCount;
		t = timeNow();
		sceNetAdhocctlDisconnect();
		sampleAdd(&dret, timeNow() - t);
		stateAt = waitState(0, 5000);
		if (waitTimedEvent(from, 5000) >= 0) {
			sampleAdd(&dev, timedEvents[from].time - t);
		}
		if (stateAt) {
			sampleAdd(&dst, stateAt - t);
		}
		sceKernelDelayThread(100000);
	}
	samplePrint("Create returns", &ret);
	samplePrint("Create -> state CONNECTED", &st);
	samplePrint("Create -> CONNECT event", &ev);
	samplePrint("Disconnect returns", &dret);
	samplePrint("Disconnect -> state DISCONNECTED", &dst);
	samplePrint("Disconnect -> DISCONNECT event", &dev);

	printf("\nScan (%d rounds, nothing around):\n", ROUNDS);
	Samples sret = { 0 }, sev = { 0 }, sst = { 0 };
	for (int r = 0; r < ROUNDS; ++r) {
		int from = timedEventCount;
		u64 t = timeNow();
		sceNetAdhocctlScan();
		sampleAdd(&sret, timeNow() - t);
		if (waitTimedEvent(from, 10000) >= 0) {
			sampleAdd(&sev, timedEvents[from].time - t);
		}
		u64 stateAt = waitState(0, 10000);
		if (stateAt) {
			sampleAdd(&sst, stateAt - t);
		}
		sceKernelDelayThread(100000);
	}
	samplePrint("Scan returns", &sret);
	samplePrint("Scan -> SCAN event", &sev);
	samplePrint("Scan -> state DISCONNECTED again", &sst);

	printf("\nCalls while disconnected:\n");
	int state, size;
	TIME_CALLS("GetState", 1000, sceNetAdhocctlGetState(&state));
	TIME_CALLS("GetScanInfo (size)", 1000, (size = 0, sceNetAdhocctlGetScanInfo(&size, NULL)));
	TIME_CALLS("GetPeerList (size)", 1000, (size = 0, sceNetAdhocctlGetPeerList(&size, NULL)));
}

static void timeSockets() {
	int from = timedEventCount;
	sceNetAdhocctlCreate("TIMING");
	waitTimedEvent(from, 5000);
	sceKernelDelayThread(200000);

	printf("\nCalls in a group:\n");
	int state, size;
	AdhocctlParameter param;
	AdhocctlPeerInfo peers[4];
	TIME_CALLS("GetState", 1000, sceNetAdhocctlGetState(&state));
	TIME_CALLS("GetParameter", 100, sceNetAdhocctlGetParameter(&param));
	for (int i = 0; i < 20; ++i) {
		u64 t = timeNow();
		sceNetAdhocctlGetParameter(&param);
		sampleAdd(&s, timeNow() - t);
	}
	samplePrint("GetParameter, one call at a time", &s);
	for (int i = 0; i < 5; ++i) {
		u64 t = timeNow();
		sceKernelDelayThread(10000);
		sampleAdd(&s, timeNow() - t);
	}
	samplePrint("sceKernelDelayThread(10000), same clock", &s);
	TIME_CALLS("GetPeerList (size)", 1000, (size = 0, sceNetAdhocctlGetPeerList(&size, NULL)));
	TIME_CALLS("GetPeerList (buffer)", 1000, (size = sizeof(peers), sceNetAdhocctlGetPeerList(&size, peers)));
	AdhocMac mac;
	TIME_CALLS("sceNetGetLocalEtherAddr", 1000, sceNetGetLocalEtherAddr(&mac));

	printf("\nPDP:\n");
	for (int i = 0; i < 20; ++i) {
		u64 t = timeNow();
		int id = sceNetAdhocPdpCreate(&localMac, PORT, 0x2000, 0);
		sampleAdd(&s, timeNow() - t);
		sceNetAdhocPdpDelete(id, 0);
	}
	samplePrint("PdpCreate", &s);
	for (int i = 0; i < 20; ++i) {
		int id = sceNetAdhocPdpCreate(&localMac, PORT, 0x2000, 0);
		u64 t = timeNow();
		sceNetAdhocPdpDelete(id, 0);
		sampleAdd(&s, timeNow() - t);
	}
	samplePrint("PdpDelete", &s);

	int pdp = sceNetAdhocPdpCreate(&localMac, PORT, 0x8000, 0);
	static u8 buf[2048];
	memset(buf, 0x55, sizeof(buf));
	TIME_CALLS("PdpSend 16 to another MAC, non-blocking", 200, sceNetAdhocPdpSend(pdp, &MAC_OTHER, PORT, buf, 16, 0, 1));
	TIME_CALLS("PdpSend 1024 to another MAC, non-blocking", 200, sceNetAdhocPdpSend(pdp, &MAC_OTHER, PORT, buf, 1024, 0, 1));
	TIME_CALLS("PdpSend 16 broadcast, non-blocking", 200, sceNetAdhocPdpSend(pdp, &MAC_BROADCAST, PORT, buf, 16, 0, 1));
	TIME_CALLS("PdpSend 16 to another MAC, blocking", 200, sceNetAdhocPdpSend(pdp, &MAC_OTHER, PORT, buf, 16, 100000, 0));
	sceKernelDelayThread(100000);
	AdhocMac src;
	u16 sport;
	int len;
	TIME_CALLS("PdpRecv, empty, non-blocking", 1000, (len = sizeof(buf), sceNetAdhocPdpRecv(pdp, &src, &sport, buf, &len, 0, 1)));
	static const u32 timeouts[] = { 1, 50, 100, 500, 1000, 5000, 10000 };
	for (int t = 0; t < 7; ++t) {
		for (int i = 0; i < 20; ++i) {
			len = sizeof(buf);
			u64 start = timeNow();
			sceNetAdhocPdpRecv(pdp, &src, &sport, buf, &len, timeouts[t], 0);
			sampleAdd(&s, timeNow() - start);
		}
		char title[64];
		snprintf(title, sizeof(title), "PdpRecv blocking, empty, timeout %u", (unsigned)timeouts[t]);
		samplePrint(title, &s);
	}
	int stsize;
	PdpStat stat[4];
	TIME_CALLS("GetPdpStat", 1000, (stsize = sizeof(stat), sceNetAdhocGetPdpStat(&stsize, stat)));

	printf("\nPollSocket (one PDP socket, polled for RECV with nothing to read):\n");
	// 0x0002 is RECV, 0x0001 SEND.
	AdhocPollSd sd = { pdp, 2, 0 };
	int pollResult = 0;
	TIME_CALLS("PollSocket, non-blocking", 1000, sceNetAdhocPollSocket(&sd, 1, 0, 1));
	for (int t = 0; t < 7; ++t) {
		for (int i = 0; i < 20; ++i) {
			sd.revents = 0;
			u64 start = timeNow();
			pollResult = sceNetAdhocPollSocket(&sd, 1, timeouts[t], 0);
			sampleAdd(&s, timeNow() - start);
		}
		char title[64];
		snprintf(title, sizeof(title), "PollSocket blocking, timeout %u (%08x %04x)", (unsigned)timeouts[t], pollResult, sd.revents);
		samplePrint(title, &s);
	}
	sd.events = 1;
	TIME_CALLS("PollSocket for SEND, non-blocking (ready)", 1000, sceNetAdhocPollSocket(&sd, 1, 0, 1));
	sceNetAdhocPdpDelete(pdp, 0);

	printf("\nPTP, connecting to itself:\n");
	for (int i = 0; i < 10; ++i) {
		u64 t = timeNow();
		int l = sceNetAdhocPtpListen(&localMac, PORT, 0x2000, 200000, 5, 2, 0);
		sampleAdd(&s, timeNow() - t);
		sceNetAdhocPtpClose(l, 0);
	}
	samplePrint("PtpListen", &s);
	Samples open = { 0 }, acc = { 0 }, conn = { 0 }, close = { 0 };
	for (int i = 0; i < 10; ++i) {
		int l = sceNetAdhocPtpListen(&localMac, PORT, 0x2000, 200000, 5, 2, 0);
		u64 t = timeNow();
		int c = sceNetAdhocPtpOpen(&localMac, PORT + 1, &localMac, PORT, 0x2000, 200000, 5, 0);
		sampleAdd(&open, timeNow() - t);
		int a = sceNetAdhocPtpAccept(l, NULL, NULL, 2000000, 0);
		sampleAdd(&acc, timeNow() - t);
		u64 t2 = timeNow();
		sceNetAdhocPtpConnect(c, 2000000, 0);
		sampleAdd(&conn, timeNow() - t2);
		t2 = timeNow();
		sceNetAdhocPtpClose(a, 0);
		sampleAdd(&close, timeNow() - t2);
		sceNetAdhocPtpClose(c, 0);
		sceNetAdhocPtpClose(l, 0);
		sceKernelDelayThread(10000);
	}
	samplePrint("PtpOpen returns", &open);
	samplePrint("PtpOpen -> blocking PtpAccept returns", &acc);
	samplePrint("PtpConnect after that", &conn);
	samplePrint("PtpClose (connected)", &close);

	int l = sceNetAdhocPtpListen(&localMac, PORT, 0x8000, 200000, 5, 2, 0);
	int c = sceNetAdhocPtpOpen(&localMac, PORT + 1, &localMac, PORT, 0x8000, 200000, 5, 0);
	int a = sceNetAdhocPtpAccept(l, NULL, NULL, 2000000, 0);
	sceNetAdhocPtpConnect(c, 2000000, 0);
	Samples rt = { 0 }, flush = { 0 };
	for (int i = 0; i < 50; ++i) {
		len = 16;
		u64 t = timeNow();
		sceNetAdhocPtpSend(c, buf, &len, 1000000, 0);
		len = sizeof(buf);
		sceNetAdhocPtpRecv(a, buf, &len, 1000000, 0);
		sampleAdd(&rt, timeNow() - t);
	}
	samplePrint("PtpSend 16 -> PtpRecv on the other end", &rt);
	for (int i = 0; i < 50; ++i) {
		len = 1024;
		u64 t = timeNow();
		sceNetAdhocPtpSend(c, buf, &len, 1000000, 0);
		int got = 0;
		while (got < 1024) {
			len = sizeof(buf);
			if (sceNetAdhocPtpRecv(a, buf, &len, 1000000, 0) < 0) {
				break;
			}
			got += len;
		}
		sampleAdd(&rt, timeNow() - t);
	}
	samplePrint("PtpSend 1024 -> all received", &rt);
	for (int i = 0; i < 20; ++i) {
		len = 1024;
		sceNetAdhocPtpSend(c, buf, &len, 1000000, 0);
		u64 t = timeNow();
		sceNetAdhocPtpFlush(c, 1000000, 0);
		sampleAdd(&flush, timeNow() - t);
		int got = 0;
		while (got < 1024) {
			len = sizeof(buf);
			if (sceNetAdhocPtpRecv(a, buf, &len, 1000000, 0) < 0) {
				break;
			}
			got += len;
		}
	}
	samplePrint("PtpFlush after sending 1024", &flush);
	TIME_CALLS("PtpRecv, empty, non-blocking", 1000, (len = sizeof(buf), sceNetAdhocPtpRecv(a, buf, &len, 0, 1)));
	TIME_CALLS("PtpSend 16, non-blocking", 200, (len = 16, sceNetAdhocPtpSend(c, buf, &len, 0, 1)));
	sceKernelDelayThread(100000);
	while (len = sizeof(buf), sceNetAdhocPtpRecv(a, buf, &len, 0, 1) == 0) {
	}
	for (int t = 0; t < 7; ++t) {
		for (int i = 0; i < 20; ++i) {
			len = sizeof(buf);
			u64 start = timeNow();
			sceNetAdhocPtpRecv(a, buf, &len, timeouts[t], 0);
			sampleAdd(&s, timeNow() - start);
		}
		char title[64];
		snprintf(title, sizeof(title), "PtpRecv blocking, empty, timeout %u", (unsigned)timeouts[t]);
		samplePrint(title, &s);
	}
	for (int t = 0; t < 7; ++t) {
		for (int i = 0; i < 20; ++i) {
			u64 start = timeNow();
			sceNetAdhocPtpAccept(l, NULL, NULL, timeouts[t], 0);
			sampleAdd(&s, timeNow() - start);
		}
		char title[64];
		snprintf(title, sizeof(title), "PtpAccept blocking, none, timeout %u", (unsigned)timeouts[t]);
		samplePrint(title, &s);
	}
	PtpStat pstat[4];
	TIME_CALLS("GetPtpStat (3 sockets)", 1000, (stsize = sizeof(pstat), sceNetAdhocGetPtpStat(&stsize, pstat)));
	sceNetAdhocPtpClose(a, 0);
	sceNetAdhocPtpClose(c, 0);
	sceNetAdhocPtpClose(l, 0);

	printf("\nPTP, connecting to a port nobody listens on:\n");
	for (int i = 0; i < 10; ++i) {
		int c = sceNetAdhocPtpOpen(&localMac, PORT + 2, &localMac, PORT + 3, 0x2000, 200000, 5, 0);
		u64 t = timeNow();
		sceNetAdhocPtpConnect(c, 2000000, 0);
		sampleAdd(&s, timeNow() - t);
		sceNetAdhocPtpClose(c, 0);
	}
	samplePrint("Blocking PtpConnect -> CONNECTION_REFUSED", &s);
}

static void timeMatching() {
	printf("\nMatching:\n");
	u64 t = timeNow();
	sceNetAdhocMatchingInit(0x20000);
	printOnce("MatchingInit", timeNow() - t);
	Samples create = { 0 }, start = { 0 }, stop = { 0 }, del = { 0 };
	for (int i = 0; i < 10; ++i) {
		t = timeNow();
		int id = sceNetAdhocMatchingCreate(1, 4, PORT + 10, 2048, 200000, 500000, 5, 200000, NULL);
		sampleAdd(&create, timeNow() - t);
		t = timeNow();
		sceNetAdhocMatchingStart(id, 0x30, 0x2000, 0x30, 0x2000, 4, "TIME");
		sampleAdd(&start, timeNow() - t);
		if (i == 0) {
			int optlen;
			char opt[16];
			int size;
			TIME_CALLS("SetHelloOpt", 200, sceNetAdhocMatchingSetHelloOpt(id, 4, "TIME"));
			TIME_CALLS("GetHelloOpt", 200, (optlen = sizeof(opt), sceNetAdhocMatchingGetHelloOpt(id, &optlen, opt)));
			TIME_CALLS("GetMembers (size)", 200, (size = 0, sceNetAdhocMatchingGetMembers(id, &size, NULL)));
		}
		sceKernelDelayThread(50000);
		t = timeNow();
		sceNetAdhocMatchingStop(id);
		sampleAdd(&stop, timeNow() - t);
		t = timeNow();
		sceNetAdhocMatchingDelete(id);
		sampleAdd(&del, timeNow() - t);
	}
	samplePrint("MatchingCreate", &create);
	samplePrint("MatchingStart", &start);
	samplePrint("MatchingStop (started 50ms before)", &stop);
	samplePrint("MatchingDelete", &del);
	t = timeNow();
	sceNetAdhocMatchingTerm();
	printOnce("MatchingTerm", timeNow() - t);
}

static void timeShutdown() {
	printf("\nShutdown:\n");
	int from = timedEventCount;
	u64 t = timeNow();
	sceNetAdhocctlDisconnect();
	printOnce("Disconnect returns", timeNow() - t);
	waitTimedEvent(from, 5000);
	t = timeNow();
	sceNetAdhocctlTerm();
	printOnce("sceNetAdhocctlTerm", timeNow() - t);
	t = timeNow();
	sceNetAdhocTerm();
	printOnce("sceNetAdhocTerm", timeNow() - t);
	t = timeNow();
	sceNetTerm();
	printOnce("sceNetTerm", timeNow() - t);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_ADHOC);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
}

// Samples the state about every millisecond after a call, and prints each change and the first event
// with its time since the call. Stops 300 ms after the event, or after 6 seconds.
static void traceStates(const char *title, int from, u64 t) {
	printf("  %s:", title);
	int last = -1;
	u64 eventAt = 0;
	u64 end = t + 6000000;
	while (timeNow() < end) {
		int state = -1;
		sceNetAdhocctlGetState(&state);
		u64 now = timeNow();
		if (state != last) {
			printf(" [%u ms: state %d]", (unsigned)((now - t) / 1000), state);
			last = state;
		}
		if (!eventAt && timedEventCount > from) {
			eventAt = timedEvents[from].time;
			printf(" [%u ms: event %d]", (unsigned)((eventAt - t) / 1000), timedEvents[from].event);
		}
		if (eventAt && now - eventAt > 300000) {
			break;
		}
		sceKernelDelayThread(1000);
	}
	printf("\n");
}

static void timeStateTrace() {
	printf("\nState changes after a call (sampled every ms):\n");
	int from = timedEventCount;
	u64 t = timeNow();
	sceNetAdhocctlCreate("TIMING");
	traceStates("Create", from, t);
	from = timedEventCount;
	t = timeNow();
	sceNetAdhocctlDisconnect();
	traceStates("Disconnect", from, t);
	sceKernelDelayThread(100000);
	from = timedEventCount;
	t = timeNow();
	sceNetAdhocctlConnect("TIMING2");
	traceStates("Connect, nobody there", from, t);
	from = timedEventCount;
	sceNetAdhocctlDisconnect();
	waitTimedEvent(from, 5000);
	sceKernelDelayThread(100000);
	from = timedEventCount;
	t = timeNow();
	sceNetAdhocctlScan();
	traceStates("Scan", from, t);

	printf("\nConnect with nobody there (%d rounds):\n", ROUNDS);
	Samples ret = { 0 }, ev = { 0 };
	for (int r = 0; r < ROUNDS; ++r) {
		from = timedEventCount;
		t = timeNow();
		sceNetAdhocctlConnect("TIMING2");
		sampleAdd(&ret, timeNow() - t);
		if (waitTimedEvent(from, 8000) >= 0 && timedEvents[from].event == 1) {
			sampleAdd(&ev, timedEvents[from].time - t);
		}
		sceKernelDelayThread(100000);
		from = timedEventCount;
		sceNetAdhocctlDisconnect();
		waitTimedEvent(from, 5000);
		sceKernelDelayThread(100000);
	}
	samplePrint("Connect returns", &ret);
	samplePrint("Connect -> CONNECT event", &ev);
}

int main(int argc, char *argv[]) {
	setvbuf(stdout, NULL, _IONBF, 0);
	timeStartup();
	timeBaseline();
	timeGroup();
	timeStateTrace();
	timeSockets();
	timeMatching();
	timeShutdown();
	return 0;
}
