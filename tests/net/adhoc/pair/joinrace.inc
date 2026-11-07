// How soon a joiner sees the host in its peer list, against how soon the host sees the joiner. The
// host creates a group, the client scans for it and joins, and the host leaves some milliseconds after
// the client shows up in its peer list; the client says whether the host ever showed up in its own.
// Legend of the Dragon's sceNetAdhocDiscover host leaves its group about 70 ms after it sees the other
// console, which then has to have seen it too. Both start in the pair group, and go back to it between
// rounds. Built as joinrace_host and joinrace_client.

#include "pair.h"

#define RACE_GROUP "JOINRACE"

static int adhocctlState() {
	int state = -1;
	int result = sceNetAdhocctlGetState(&state);
	return result < 0 ? result : state;
}

static int peerCount() {
	AdhocctlPeerInfo peers[4];
	int size = sizeof(peers);
	if (sceNetAdhocctlGetPeerList(&size, peers) < 0) {
		return -1;
	}
	return size / (int)sizeof(AdhocctlPeerInfo);
}

static void leaveIfInGroup() {
	int state = adhocctlState();
	if (state == 1) {
		adhocctlLeaveGroup();
	} else if (state != 0) {
		sceNetAdhocctlDisconnect();
		sceKernelDelayThread(500000);
	}
}

// Each case takes the same time on both consoles, however soon either side is done.
static void waitUntil(u64 begin, int seconds) {
	u64 end = begin + (u64)seconds * 1000000;
	u64 now = sceKernelGetSystemTimeWide();
	if (now < end) {
		sceKernelDelayThread((SceUInt)(end - now));
	}
}

static void backToPairGroup(int step) {
	leaveIfInGroup();
	sceKernelDelayThread(2000000);
	if (pairJoin() && pairFindPeer() == 1) {
		pairSync(step);
	} else {
		printf("  Couldn't get back to the pair group\n");
	}
}

// One round: the host creates the group, the client scans for it and joins, and the host leaves
// delayMs after the client shows up in its peer list. The client says whether the host ever showed up
// in its own. Nothing is printed until the round is over: output over PSPLink is slow.
static void joinRace(int delayMs, int step) {
	pairSync(step);
	u64 caseBegin = sceKernelGetSystemTimeWide();
	leaveIfInGroup();
#if HOST
	sceKernelDelayThread(500000);
	int start = eventCount;
	sceNetAdhocctlCreate(RACE_GROUP);
	waitEvent(start, 5000);
	u64 begin = sceKernelGetSystemTimeWide();
	int peers = 0;
	while (sceKernelGetSystemTimeWide() - begin < 8000000 && peers <= 0) {
		peers = peerCount();
		if (peers <= 0) {
			sceKernelDelayThread(1000);
		}
	}
	if (peers > 0) {
		if (delayMs > 0) {
			sceKernelDelayThread(delayMs * 1000);
		}
		sceNetAdhocctlDisconnect();
	}
	printf("  Leave %d ms after the client shows up: client %s\n", delayMs, peers > 0 ? "showed up" : "didn't show up");
#else
	sceKernelDelayThread(2000000);
	AdhocctlScanInfo info;
	int found = 0;
	for (int scans = 0; scans < 5 && !found; ++scans) {
		int start = eventCount;
		if (sceNetAdhocctlScan() < 0 || waitEvent(start, 3000) != 3) {
			sceKernelDelayThread(200000);
			continue;
		}
		AdhocctlScanInfo infos[8];
		int size = sizeof(infos);
		if (sceNetAdhocctlGetScanInfo(&size, infos) >= 0) {
			for (int i = 0; i < size / (int)sizeof(AdhocctlScanInfo) && i < 8; ++i) {
				if (memcmp(infos[i].group, RACE_GROUP, 8) == 0) {
					info = infos[i];
					found = 1;
				}
			}
		}
	}
	int seen = 0;
	if (found) {
		sceNetAdhocctlJoin(&info);
		u64 begin = sceKernelGetSystemTimeWide();
		while (sceKernelGetSystemTimeWide() - begin < 4000000 && !seen) {
			seen = peerCount() > 0;
			if (!seen) {
				sceKernelDelayThread(1000);
			}
		}
	}
	printf("  Host leaves %d ms after it sees us: %s\n", delayMs, !found ? "group not found" : (seen ? "saw the host" : "never saw the host"));
#endif
	leaveIfInGroup();
	waitUntil(caseBegin, 13);
	backToPairGroup(step + 1);
}

static void joinRaces(int step) {
	static const int delays[] = { 0, 25, 50, 100, 200, 400, 800 };
	printf("\nHost leaves some time after the client shows up, client joins:\n");
	for (int i = 0; i < (int)(sizeof(delays) / sizeof(delays[0])); ++i) {
		joinRace(delays[i], step + i * 2);
	}
}

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	joinRaces(10);
	pairShutdown();
	return 0;
}
