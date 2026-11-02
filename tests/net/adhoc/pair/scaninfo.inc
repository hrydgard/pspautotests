// What sceNetAdhocctlGetScanInfo returns around joining a group: before any scan, after a scan, after
// Create (host) or Join (client), a few seconds later, after leaving and joining again (client), and
// after a scan attempt while connected. Built as scaninfo_host and scaninfo_client.

#include "pair.h"

static void printScan(const char *label) {
	int querySize = 0;
	int queryResult = sceNetAdhocctlGetScanInfo(&querySize, NULL);
	AdhocctlScanInfo infos[8];
	memset(infos, 0xCC, sizeof(infos));
	int size = sizeof(infos);
	int result = sceNetAdhocctlGetScanInfo(&size, infos);
	int n = result >= 0 ? size / (int)sizeof(AdhocctlScanInfo) : 0;
	printf("%s: size query %08x, %d entries; GetScanInfo %08x, %d entries\n", label, queryResult,
		queryResult >= 0 ? querySize / (int)sizeof(AdhocctlScanInfo) : -1, result, n);

	AdhocctlParameter param;
	memset(&param, 0, sizeof(param));
	int state = -1;
	sceNetAdhocctlGetState(&state);
	int haveParam = state != 0 && sceNetAdhocctlGetParameter(&param) >= 0;
	for (int i = 0; i < n && i < 8; ++i) {
		if (memcmp(infos[i].group, PAIR_GROUP, 8) != 0) {
			continue;
		}
		printf("  PAIRTEST: mode %d, channel %s", infos[i].mode, infos[i].channel == 1 || infos[i].channel == 6 || infos[i].channel == 11 ? "1/6/11" : "unexpected");
		if (haveParam) {
			printf(", bssid is the group's: %s", memcmp(&infos[i].bssid, &param.bssid, 6) == 0 ? "yes" : "no");
		}
		printf("\n");
	}
}

static void scanOnce(const char *label) {
	int start = eventCount;
	int result = sceNetAdhocctlScan();
	int event = result < 0 ? -1 : waitEvent(start, 5000);
	int state = -1;
	sceNetAdhocctlGetState(&state);
	printf("%s: Scan %08x, event %d (error %08x), state %d\n", label, result, event, event >= 0 ? lastError : 0, state);
}

int main(int argc, char *argv[]) {
	(void)pairStartup;
	if (!adhocStartup()) {
		adhocShutdown();
		return 1;
	}
	printf("sceNetAdhocInit: %08x\n", sceNetAdhocInit());
	printf("Role: %s\n", HOST ? "host" : "client");
	if (!adhocctlStartup()) {
		adhocShutdown();
		return 1;
	}
	handlerId = sceNetAdhocctlAddHandler(&adhocctlHandler, NULL);
	printScan("Before any scan");
#if HOST
	// The client is scanning too, but neither side has a group yet.
	scanOnce("Host scan");
	printScan("After a scan");
#endif
	if (!pairJoin()) {
		pairShutdown();
		return 1;
	}
#if HOST
	printScan("After Create");
#else
	printScan("After Join");
#endif
	int peers = pairFindPeer();
	printf("Peers: %d\n", peers);
	if (peers != 1) {
		pairShutdown();
		return 1;
	}
	printScan("With the peer listed");
	pairSync(1);
	sceKernelDelayThread(3000000);
	printScan("Three seconds later");
	pairSync(2);
#if HOST
	// The client leaves and joins again meanwhile.
	pairSync(3);
#else
	adhocctlLeaveGroup();
	printScan("After leaving");
	if (!pairJoin()) {
		pairShutdown();
		return 1;
	}
	printScan("After joining again");
	pairSync(3);
#endif
	scanOnce("Scan while connected");
	printScan("After it");
	pairSync(4);
	pairShutdown();
	return 0;
}
