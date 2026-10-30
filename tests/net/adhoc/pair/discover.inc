// sceNetAdhocDiscover with another console around. Both start in the pair group and leave it. Then the
// host creates a group the normal way while the client discovers it (with Legend of the Dragon's
// parameters): the client prints how its status and adhocctl state change, the result, how long it took,
// the handler events, and where it ends up. Then both discover the same group name at once; how long they
// take to find each other varies a lot (5 to over 25 seconds, the same steps with more scans), so only
// whether they do within a minute and where they end up is printed. Between cases both go back to the pair
// group. (With unknown1 1, as Dissidia 012 does, adhocctl goes into state 5 and stays there; it seems to
// wait for something else, so it isn't tried.) Built as discover_host and discover_client.

#include "pair.h"

typedef struct {
	u32 unknown1;
	char group[8];
	u32 unknown2;
	unsigned int result;
} DiscoverParam;

int sceNetAdhocDiscoverInitStart(DiscoverParam *param);
int sceNetAdhocDiscoverUpdate(void);
int sceNetAdhocDiscoverGetStatus(void);
int sceNetAdhocDiscoverTerm(void);
int sceNetAdhocDiscoverStop(void);

#define DISCOVER_GROUP "DISCPAIR"

static int adhocctlState() {
	int state = -1;
	int result = sceNetAdhocctlGetState(&state);
	return result < 0 ? result : state;
}

static const char *timeBucket(u64 us) {
	if (us < 1000000)
		return "under 1s";
	if (us < 2000000)
		return "1-2s";
	if (us < 5000000)
		return "2-5s";
	if (us < 10000000)
		return "5-10s";
	if (us < 20000000)
		return "10-20s";
	return "20s or more";
}

// Calls Update every 16 ms for up to timeoutMs, until the status is 2. Prints each status/state pair the
// first time it's seen: how many times adhocctl goes scanning before the two find each other varies.
static int runDiscover(int timeoutMs) {
	int seen[16][2];
	int numSeen = 0;
	int status = 0;
	printf("  status/state:");
	for (int i = 0; i < timeoutMs / 16; ++i) {
		int update = sceNetAdhocDiscoverUpdate();
		status = sceNetAdhocDiscoverGetStatus();
		int state = adhocctlState();
		int known = 0;
		for (int j = 0; j < numSeen; ++j) {
			if (seen[j][0] == status && seen[j][1] == state) {
				known = 1;
			}
		}
		if (!known && numSeen < 16) {
			seen[numSeen][0] = status;
			seen[numSeen][1] = state;
			numSeen++;
			printf(" %d/%d", status, state);
			if (update != 0) {
				printf("(update %08x)", update);
			}
		}
		if (status == 2) {
			break;
		}
		sceKernelDelayThread(16000);
	}
	printf("\n");
	return status;
}

static int peerCount() {
	AdhocctlPeerInfo peers[4];
	int size = sizeof(peers);
	if (sceNetAdhocctlGetPeerList(&size, peers) < 0) {
		return -1;
	}
	return size / (int)sizeof(AdhocctlPeerInfo);
}

static void printWhereItIs() {
	AdhocctlParameter param;
	memset(&param, 0, sizeof(param));
	int result = sceNetAdhocctlGetParameter(&param);
	char group[9] = {};
	memcpy(group, param.group, 8);
	const char *bssid = memcmp(&param.bssid, &localMac, 6) == 0 ? "own MAC" : (memcmp(&param.bssid, &peerMac, 6) == 0 ? "peer's MAC" : "other");
	// The peer may take a few seconds to show up.
	int peers = peerCount();
	for (int i = 0; i < 100 && peers == 0; ++i) {
		sceKernelDelayThread(100000);
		peers = peerCount();
	}
	printf("  adhocctl state %d, GetParameter %08x, group %s, BSSID %s, peers %d\n", adhocctlState(), result, group, bssid, peers);
}

// The handler events since eventCount was last reset, with repeats in a row printed once.
static void printEvents() {
	printf("  Handler events:");
	for (int i = 0; i < eventCount && i < 16; ++i) {
		if (i > 0 && eventLog[i][0] == eventLog[i - 1][0] && eventLog[i][1] == eventLog[i - 1][1]) {
			continue;
		}
		printf(" %d(%08x)", eventLog[i][0], eventLog[i][1]);
	}
	printf("\n");
}

static void discover(u32 unknown1, u32 unknown2, int detailed) {
	DiscoverParam param = { unknown1, { 'D', 'I', 'S', 'C', 'P', 'A', 'I', 'R' }, unknown2, 0xCCCC };
	eventCount = 0;
	u64 begin = sceKernelGetSystemTimeWide();
	printf("  InitStart: %08x\n", sceNetAdhocDiscoverInitStart(&param));
	int status;
	if (detailed) {
		status = runDiscover(25000);
		u64 took = sceKernelGetSystemTimeWide() - begin;
		printf("  Status %d, result field %x, %s\n", status, param.result, status == 2 ? timeBucket(took) : "not done in 25s");
	} else {
		status = 0;
		for (int i = 0; i < 60000 / 16 && status != 2; ++i) {
			sceNetAdhocDiscoverUpdate();
			status = sceNetAdhocDiscoverGetStatus();
			sceKernelDelayThread(16000);
		}
		printf("  Status %d, result field %x\n", status, status == 2 ? param.result : 0xCCCC);
	}
	if (status != 2) {
		printf("  Stop: %08x\n", sceNetAdhocDiscoverStop());
		status = runDiscover(5000);
		printf("  After Stop: status %d, result field %x\n", status, param.result);
	}
	if (detailed) {
		printEvents();
	}
	printWhereItIs();
	printf("  Term: %08x, adhocctl state %d\n", sceNetAdhocDiscoverTerm(), adhocctlState());
}

static void leaveIfInGroup() {
	if (adhocctlState() != 0) {
		adhocctlLeaveGroup();
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

static void bothDiscover(const char *title, u32 unknown1, u32 unknown2, int step) {
	printf("\n%s:\n", title);
	pairSync(step);
	leaveIfInGroup();
	sceKernelDelayThread(1000000);
	discover(unknown1, unknown2, 0);
	backToPairGroup(step + 1);
}

static void hostCreatesClientDiscovers(int step) {
	printf("\nHost creates the group, client discovers it:\n");
	pairSync(step);
	leaveIfInGroup();
#if HOST
	adhocctlCreateGroup(DISCOVER_GROUP);
	eventCount = 0;
	// Wait for the client to show up, for up to 25 seconds.
	u64 begin = sceKernelGetSystemTimeWide();
	int peers = 0;
	for (int i = 0; i < 250 && peers == 0; ++i) {
		sceKernelDelayThread(100000);
		peers = peerCount();
	}
	printf("  Client joined: %s\n", peers > 0 ? timeBucket(sceKernelGetSystemTimeWide() - begin) : "not within 25s");
	// Give the client time to finish its side.
	sceKernelDelayThread(5000000);
	printEvents();
#else
	sceKernelDelayThread(2000000);
	discover(0, 0x3c, 1);
#endif
	backToPairGroup(step + 1);
}

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	hostCreatesClientDiscovers(10);
	bothDiscover("Both discover (Legend of the Dragon's parameters)", 0, 0x3c, 20);
	pairShutdown();
	return 0;
}
