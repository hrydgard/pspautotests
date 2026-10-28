// Timing between two consoles: PDP and PTP round trips, PTP connects, matching (hello intervals,
// joining, data), game mode (entering, how often replicas update) and scanning and joining a group.
// The two clocks aren't synchronized, so everything is measured on one console, mostly as round
// trips. A measurement, not a regression test. Built as timing_host and timing_client.

#include "pairmatching.h"
#include "../timing.h"

#define PDP_PORT 0x5100
#define PTP_PORT 0x5200
#define MATCHING_PORT 0x5300
#define GM_GROUP "GMTIMING"

static u8 buf[0x4000];
static Samples s;

static const int pdpSizes[] = { 16, 256, 1024, 1444, 4096, 8192 };
#define NUM_PDP_SIZES 6
#define PDP_ROUNDS 30

// The client sends, the host sends each datagram back. A first byte of 0xEE ends the echoing.
static void pdpRoundTrips() {
	int pdp = sceNetAdhocPdpCreate(&localMac, PDP_PORT, 0x8000, 0);
	pairSync(10);
#if HOST
	// Until the end marker, or 5 seconds without anything.
	u64 end = timeNow() + 90000000;
	u64 lastData = timeNow();
	while (timeNow() < end && timeNow() - lastData < 5000000) {
		AdhocMac from;
		u16 port;
		int len = sizeof(buf);
		if (sceNetAdhocPdpRecv(pdp, &from, &port, buf, &len, 1000000, 0) < 0) {
			continue;
		}
		lastData = timeNow();
		if (buf[0] == 0xEE) {
			break;
		}
		sceNetAdhocPdpSend(pdp, &peerMac, PDP_PORT, buf, len, 0, 1);
	}
	printf("  (echoed)\n");
#else
	// Each datagram carries a sequence number, and echoes of earlier ones (late, or from a round that
	// gave up) are skipped.
	// Polling never gets anything back unless the thread yields between attempts: the net stack's
	// threads run at the same priority as this one (sceNetInit's 0x20).
	static const char *modes[] = { "blocking recv", "polled, DelayThread(0)", "polled, DelayThread(100)" };
	u32 seq = 0;
	for (int spin = 0; spin < 3; ++spin) {
		for (int z = 0; z < NUM_PDP_SIZES; ++z) {
			int lost = 0;
			for (int i = 0; i < PDP_ROUNDS; ++i) {
				memset(buf, i, pdpSizes[z]);
				buf[0] = 1;
				++seq;
				memcpy(buf + 4, &seq, 4);
				u64 t = timeNow();
				sceNetAdhocPdpSend(pdp, &peerMac, PDP_PORT, buf, pdpSizes[z], 0, 1);
				u64 giveUp = t + 1000000;
				int got = 0;
				while (!got && timeNow() < giveUp) {
					AdhocMac from;
					u16 port;
					int len = sizeof(buf);
					int r = spin ? sceNetAdhocPdpRecv(pdp, &from, &port, buf, &len, 0, 1) : sceNetAdhocPdpRecv(pdp, &from, &port, buf, &len, 1000000, 0);
					if (spin && r != 0) {
						sceKernelDelayThread(spin == 1 ? 0 : 100);
					}
					u32 back;
					memcpy(&back, buf + 4, 4);
					if (r == 0 && len >= 8 && back == seq) {
						got = 1;
					}
				}
				if (got) {
					sampleAdd(&s, timeNow() - t);
				} else {
					lost++;
				}
				sceKernelDelayThread(2000);
			}
			char title[64];
			snprintf(title, sizeof(title), "PDP %5d, %s (%d lost)", pdpSizes[z], modes[spin], lost);
			samplePrint(title, &s);
		}
	}
	buf[0] = 0xEE;
	for (int i = 0; i < 20; ++i) {
		sceNetAdhocPdpSend(pdp, &peerMac, PDP_PORT, buf, 8, 0, 1);
		sceKernelDelayThread(10000);
	}
#endif
	pairSync(11);
	sceNetAdhocPdpDelete(pdp, 0);
}

static inline int ptpRecvAll(int id, int want) {
	int got = 0;
	while (got < want) {
		int len = sizeof(buf);
		if (sceNetAdhocPtpRecv(id, buf, &len, 2000000, 0) < 0) {
			return got;
		}
		got += len;
	}
	return got;
}

static const int ptpSizes[] = { 16, 1024, 8192 };

static void ptpTiming() {
#if HOST
	int l = sceNetAdhocPtpListen(&localMac, PTP_PORT, 0x8000, 200000, 5, 4, 0);
	pairSync(20);
	Samples accept = { 0 };
	for (int i = 0; i < 10; ++i) {
		u64 t = timeNow();
		int a = sceNetAdhocPtpAccept(l, NULL, NULL, 5000000, 0);
		if (a > 0) {
			sampleAdd(&accept, timeNow() - t);
		}
		pairSync(21 + i);
		if (a > 0) {
			sceNetAdhocPtpClose(a, 0);
		}
	}
	samplePrint("Accept waiting since the client's sync", &accept);
	int a = sceNetAdhocPtpAccept(l, NULL, NULL, 5000000, 0);
	// Echo: what arrives goes back, until 0xEE.
	u64 end = timeNow() + 60000000;
	u64 lastData = timeNow();
	while (a > 0 && timeNow() < end && timeNow() - lastData < 5000000) {
		int len = sizeof(buf);
		if (sceNetAdhocPtpRecv(a, buf, &len, 1000000, 0) < 0) {
			continue;
		}
		lastData = timeNow();
		if (buf[0] == 0xEE) {
			break;
		}
		int sendLen = len;
		sceNetAdhocPtpSend(a, buf, &sendLen, 1000000, 0);
	}
	printf("  (echoed)\n");
	pairSync(40);
	if (a > 0) {
		sceNetAdhocPtpClose(a, 0);
	}
	sceNetAdhocPtpClose(l, 0);
#else
	pairSync(20);
	Samples open = { 0 }, connect = { 0 };
	for (int i = 0; i < 10; ++i) {
		u64 t = timeNow();
		int c = sceNetAdhocPtpOpen(&localMac, 0, &peerMac, PTP_PORT, 0x8000, 200000, 5, 0);
		sampleAdd(&open, timeNow() - t);
		if (sceNetAdhocPtpConnect(c, 5000000, 0) == 0) {
			sampleAdd(&connect, timeNow() - t);
		}
		pairSync(21 + i);
		sceNetAdhocPtpClose(c, 0);
	}
	samplePrint("PtpOpen returns", &open);
	samplePrint("PtpOpen -> blocking PtpConnect returns", &connect);
	int c = sceNetAdhocPtpOpen(&localMac, 0, &peerMac, PTP_PORT, 0x8000, 200000, 5, 0);
	sceNetAdhocPtpConnect(c, 5000000, 0);
	for (int z = 0; z < 3; ++z) {
		int short_ = 0;
		for (int i = 0; i < 30; ++i) {
			memset(buf, 1, ptpSizes[z]);
			int len = ptpSizes[z];
			u64 t = timeNow();
			sceNetAdhocPtpSend(c, buf, &len, 2000000, 0);
			if (ptpRecvAll(c, ptpSizes[z]) == ptpSizes[z]) {
				sampleAdd(&s, timeNow() - t);
			} else {
				short_++;
			}
		}
		char title[64];
		snprintf(title, sizeof(title), "PTP %5d bytes round trip (%d short)", ptpSizes[z], short_);
		samplePrint(title, &s);
	}
	for (int i = 0; i < 10; ++i) {
		memset(buf, 1, 8192);
		int len = 8192;
		sceNetAdhocPtpSend(c, buf, &len, 2000000, 0);
		u64 t = timeNow();
		sceNetAdhocPtpFlush(c, 2000000, 0);
		sampleAdd(&s, timeNow() - t);
		ptpRecvAll(c, 8192);
	}
	samplePrint("PtpFlush after sending 8192", &s);
	buf[0] = 0xEE;
	int len = 4;
	sceNetAdhocPtpSend(c, buf, &len, 1000000, 0);
	pairSync(40);
	sceNetAdhocPtpClose(c, 0);
#endif
}

// Every matching event with its time. pairmatching.h's list folds hellos and holds 48, too few here.
typedef struct {
	int event;
	u64 time;
} LoggedEvent;

static volatile LoggedEvent mlog[256];
static volatile int mlogCount = 0;

static void timingMatchingHandler(int id, int event, AdhocMac *mac, int optlen, void *opt) {
	if (mlogCount < 256) {
		mlog[mlogCount].event = event;
		mlog[mlogCount].time = timeNow();
		mlogCount++;
	}
}

// The first event of the type at or after `from`, waiting up to timeoutMs. Returns its index or -1.
static inline int waitLogged(int event, int from, int timeoutMs) {
	u64 end = timeNow() + (u64)timeoutMs * 1000;
	do {
		for (int i = from; i < mlogCount; ++i) {
			if (mlog[i].event == event) {
				return i;
			}
		}
		sceKernelDelayThread(100);
	} while (timeNow() < end);
	return -1;
}

static inline void helloIntervals(const char *title, int from) {
	u64 last = 0;
	for (int i = from; i < mlogCount; ++i) {
		if (mlog[i].event == 1) {
			if (last) {
				sampleAdd(&s, mlog[i].time - last);
			}
			last = mlog[i].time;
		}
	}
	samplePrint(title, &s);
}

static void matchingTiming() {
	sceNetAdhocMatchingInit(0x20000);
	static const u32 helloInts[] = { 200000, 500000 };
	for (int h = 0; h < 2; ++h) {
		mlogCount = 0;
#if HOST
		int id = sceNetAdhocMatchingCreate(1, 4, MATCHING_PORT + h, 2048, helloInts[h], 500000, 5, 200000, &timingMatchingHandler);
		printf("  Host context: %s\n", id > 0 ? "ok" : "failed");
		sceNetAdhocMatchingStart(id, 0x30, 0x2000, 0x30, 0x2000, 4, "TIME");
		pairSync(50 + h * 10);
		// The client times the hellos for 4 seconds.
		pairSync(51 + h * 10);
#else
		int id = sceNetAdhocMatchingCreate(2, 4, MATCHING_PORT + h, 2048, helloInts[h], 500000, 5, 200000, &timingMatchingHandler);
		printf("  Client context: %s\n", id > 0 ? "ok" : "failed");
		sceNetAdhocMatchingStart(id, 0x30, 0x2000, 0x30, 0x2000, 0, NULL);
		pairSync(50 + h * 10);
		int helloFrom = mlogCount;
		sceKernelDelayThread(4000000);
		char title[64];
		snprintf(title, sizeof(title), "Interval between hellos, hello_int %u", (unsigned)helloInts[h]);
		helloIntervals(title, helloFrom);
		pairSync(51 + h * 10);
#endif
		if (h == 1) {
			sceNetAdhocMatchingStop(id);
			sceNetAdhocMatchingDelete(id);
			continue;
		}

		// Joining.
#if HOST
		int req = waitLogged(2, 0, 5000);
		printf("  Host got REQUEST: %s\n", req >= 0 ? "yes" : "no");
		if (req >= 0) {
			u64 t = mlog[req].time;
			int r = sceNetAdhocMatchingSelectTarget(id, &peerMac, 0, NULL);
			printOnce("Host: REQUEST event -> SelectTarget returned", timeNow() - t);
			int est = waitLogged(7, req, 5000);
			printf("  Host SelectTarget %08x, ESTABLISHED: %s\n", r, est >= 0 ? "yes" : "no");
			if (est >= 0) {
				printOnce("Host: REQUEST -> ESTABLISHED", mlog[est].time - t);
			}
		}
		pairSync(52);
		// Each DATA gets an answer, for up to 20 seconds or 20 answers.
		int seen = mlogCount;
		u64 end = timeNow() + 20000000;
		int answered = 0;
		while (answered < 20 && timeNow() < end) {
			for (; seen < mlogCount; ++seen) {
				if (mlog[seen].event == 11) {
					sceNetAdhocMatchingSendData(id, &peerMac, 4, "PONG");
					answered++;
				}
			}
			sceKernelDelayThread(100);
		}
		printf("  Host answered %d\n", answered);
		pairSync(53);
#else
		int start = mlogCount;
		u64 t = timeNow();
		int r = sceNetAdhocMatchingSelectTarget(id, &peerMac, 0, NULL);
		int est = waitLogged(7, start, 5000);
		printf("  Client SelectTarget %08x, ESTABLISHED: %s\n", r, est >= 0 ? "yes" : "no");
		if (est >= 0) {
			printOnce("Client: SelectTarget -> ESTABLISHED", mlog[est].time - t);
		}
		pairSync(52);
		Samples ack = { 0 }, reply = { 0 }, call = { 0 };
		int failed = 0;
		for (int i = 0; i < 20; ++i) {
			int from = mlogCount;
			u64 t = timeNow();
			if (sceNetAdhocMatchingSendData(id, &peerMac, 4, "PING") < 0) {
				failed++;
			}
			sampleAdd(&call, timeNow() - t);
			int a = waitLogged(12, from, 3000);
			int d = waitLogged(11, from, 3000);
			if (a >= 0) {
				sampleAdd(&ack, mlog[a].time - t);
			}
			if (d >= 0) {
				sampleAdd(&reply, mlog[d].time - t);
			}
			sceKernelDelayThread(50000);
		}
		printf("  SendData failed %d times\n", failed);
		samplePrint("SendData returns", &call);
		samplePrint("SendData -> DATA_ACK", &ack);
		samplePrint("SendData -> the host's reply DATA", &reply);
		pairSync(53);
#endif
		sceNetAdhocMatchingStop(id);
		sceNetAdhocMatchingDelete(id);
	}
	sceNetAdhocMatchingTerm();
}

typedef struct {
	u32 counter;
	u8 fill[60];
} GameData;

typedef struct {
	u32 length;
	u32 updated;
	u64 timestamp;
} ReplicaInfo;

static GameData master, replica;

static void gameModeTiming() {
	pairSync(70);
	adhocctlLeaveGroup();
	sceNetAdhocctlAddHandler(&timedHandler, NULL);
	int from = timedEventCount;
	u64 t;
#if HOST
	AdhocMac members[2] = { localMac, peerMac };
	sceKernelDelayThread(500000);
	t = timeNow();
	int result = sceNetAdhocctlCreateEnterGameMode(GM_GROUP, 1, 2, members, 10000000, 0);
#else
	sceKernelDelayThread(1500000);
	t = timeNow();
	int result = sceNetAdhocctlJoinEnterGameMode(GM_GROUP, &peerMac, 10000000, 0);
#endif
	if (result < 0 || waitTimedEvent(from, 12000) < 0 || timedEvents[from].event != 4) {
		printf("  Game mode: %08x, no GAMEMODE event\n", result);
		return;
	}
	printOnce(HOST ? "CreateEnterGameMode -> GAMEMODE event" : "JoinEnterGameMode -> GAMEMODE event", timedEvents[from].time - t);

	memset(&master, 0, sizeof(master));
	sceNetAdhocGameModeCreateMaster(&master, sizeof(master));
	int rep = sceNetAdhocGameModeCreateReplica(&peerMac, &replica, sizeof(replica));
	TIME_CALLS("UpdateMaster", 100, sceNetAdhocGameModeUpdateMaster());
	ReplicaInfo info;
	TIME_CALLS("UpdateReplica", 100, (info.length = sizeof(info), sceNetAdhocGameModeUpdateReplica(rep, &info)));

	// Both update their master every 1ms and poll the replica every 200us for 4 seconds, noting when
	// the peer's counter changes.
	u64 end = timeNow() + 4000000;
	u64 lastMaster = 0, lastChange = 0;
	u32 lastCounter = 0;
	int changes = 0, skipped = 0;
	while (timeNow() < end) {
		u64 now = timeNow();
		if (now - lastMaster >= 1000) {
			master.counter++;
			sceNetAdhocGameModeUpdateMaster();
			lastMaster = now;
		}
		info.length = sizeof(info);
		sceNetAdhocGameModeUpdateReplica(rep, &info);
		if (replica.counter != lastCounter) {
			if (lastChange != 0) {
				sampleAdd(&s, now - lastChange);
				skipped += replica.counter - lastCounter - 1;
			}
			lastChange = now;
			lastCounter = replica.counter;
			changes++;
		}
		sceKernelDelayThread(200);
	}
	char title[80];
	snprintf(title, sizeof(title), "Replica changes (%d, %d master updates skipped)", changes, skipped);
	samplePrint(title, &s);
	printf("  Own master updates in 4s: %u\n", (unsigned)master.counter);

	sceNetAdhocGameModeDeleteReplica(rep);
	sceNetAdhocGameModeDeleteMaster();
	from = timedEventCount;
	t = timeNow();
	sceNetAdhocctlExitGameMode();
	if (waitTimedEvent(from, 5000) >= 0) {
		printOnce("ExitGameMode -> event", timedEvents[from].time - t);
	}
	waitState(0, 5000);
}

// Last, as there's no group to sync through while it runs: the host creates the group, the client
// scans for it and joins, three times.
// Rounds 0-2 scan and join; in rounds 3-4 the client calls Connect on the group instead.
static void joinTiming() {
	for (int round = 0; round < 5; ++round) {
		int from = timedEventCount;
#if HOST
		u64 t = timeNow();
		sceNetAdhocctlCreate(PAIR_GROUP);
		if (waitTimedEvent(from, 5000) >= 0) {
			sampleAdd(&s, timedEvents[from].time - t);
		}
		u64 connected = timeNow();
		// Wait for the client, then for it to leave.
		AdhocctlPeerInfo peers[4];
		int size = 0;
		u64 end = timeNow() + 30000000;
		while (timeNow() < end) {
			size = sizeof(peers);
			sceNetAdhocctlGetPeerList(&size, peers);
			if (size > 0) {
				break;
			}
			sceKernelDelayThread(1000);
		}
		printf("  Round %d: client in the peer list %s after CONNECT\n", round, size > 0 ? "" : "never");
		end = timeNow() + 30000000;
		while (timeNow() < end) {
			size = sizeof(peers);
			sceNetAdhocctlGetPeerList(&size, peers);
			if (size == 0) {
				break;
			}
			sceKernelDelayThread(1000);
		}
		(void)connected;
		from = timedEventCount;
		sceNetAdhocctlDisconnect();
		waitTimedEvent(from, 5000);
		sceKernelDelayThread(2000000);
#else
		sceKernelDelayThread(1000000);
		if (round >= 3) {
			// The host's group is up by now (its CONNECT takes about 1.1 s).
			sceKernelDelayThread(1000000);
			from = timedEventCount;
			u64 t = timeNow();
			sceNetAdhocctlConnect(PAIR_GROUP);
			if (waitTimedEvent(from, 8000) >= 0) {
				printOnce("Connect to the group -> CONNECT event", timedEvents[from].time - t);
			}
			sceKernelDelayThread(500000);
			from = timedEventCount;
			sceNetAdhocctlDisconnect();
			waitTimedEvent(from, 5000);
			sceKernelDelayThread(1000000);
			continue;
		}
		Samples scan = { 0 };
		AdhocctlScanInfo info;
		int found = 0, scans = 0;
		u64 firstScan = timeNow();
		while (!found && scans < 15) {
			from = timedEventCount;
			u64 t = timeNow();
			sceNetAdhocctlScan();
			if (waitTimedEvent(from, 5000) >= 0) {
				sampleAdd(&scan, timedEvents[from].time - t);
			}
			waitState(0, 5000);
			found = findGroup(&info);
			scans++;
		}
		printf("  Round %d: found the group after %d scan(s), %u us\n", round, scans, (unsigned)(timeNow() - firstScan));
		samplePrint("Scan -> SCAN event", &scan);
		if (!found) {
			continue;
		}
		from = timedEventCount;
		u64 t = timeNow();
		sceNetAdhocctlJoin(&info);
		if (waitTimedEvent(from, 5000) >= 0) {
			printOnce("Join -> CONNECT event", timedEvents[from].time - t);
		}
		AdhocctlPeerInfo peers[4];
		int size = 0;
		u64 end = timeNow() + 10000000;
		while (timeNow() < end) {
			size = sizeof(peers);
			sceNetAdhocctlGetPeerList(&size, peers);
			if (size > 0) {
				break;
			}
			sceKernelDelayThread(1000);
		}
		if (size > 0) {
			printOnce("Join -> host in the peer list", timeNow() - t);
		}
		sceKernelDelayThread(500000);
		from = timedEventCount;
		sceNetAdhocctlDisconnect();
		waitTimedEvent(from, 5000);
		sceKernelDelayThread(1000000);
#endif
	}
#if HOST
	samplePrint("Create -> CONNECT event", &s);
#endif
}

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	printf("\nPDP:\n");
	pdpRoundTrips();
	printf("\nPTP:\n");
	ptpTiming();
	printf("\nMatching:\n");
	matchingTiming();
	printf("\nGame mode:\n");
	gameModeTiming();
	printf("\nScanning and joining:\n");
	joinTiming();
	adhocShutdown();
	return 0;
}
