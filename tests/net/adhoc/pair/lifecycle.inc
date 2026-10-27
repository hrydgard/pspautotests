// What happens to a group when the console that created it leaves: what the one left behind sees,
// whether the creator can find the network again and rejoin it, and what two separately created
// groups with the same name do. Built as lifecycle_host and lifecycle_client.

#include "pairmatching.h"

static int peerCount() {
	int size = 0;
	sceNetAdhocctlGetPeerList(&size, NULL);
	return size / (int)sizeof(AdhocctlPeerInfo);
}

// Waits up to timeoutMs for the peer list to have `count` entries. Returns how long it took, or -1.
static s64 waitPeers(int count, int timeoutMs) {
	u64 begin = sceKernelGetSystemTimeWide();
	for (int i = 0; i < timeoutMs / 100; ++i) {
		if (peerCount() == count) {
			return sceKernelGetSystemTimeWide() - begin;
		}
		sceKernelDelayThread(100000);
	}
	return -1;
}

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	AdhocctlParameter param;
	sceNetAdhocctlGetParameter(&param);
	AdhocMac originalBssid = param.bssid;
	pairSync(1);

	printf("\nThe creator leaves:\n");
#if HOST
	adhocctlLeaveGroup();
	// Longer than the client takes to time the host out, in case it misses the leave.
	sceKernelDelayThread(17000000);

	printf("\nFinding the network again:\n");
	AdhocctlScanInfo infos[8];
	int found = 0, matches = 0;
	for (int tries = 0; tries < 5 && !found; ++tries) {
		int start = eventCount;
		if (sceNetAdhocctlScan() < 0 || waitEvent(start, 3000) != 3) {
			continue;
		}
		int size = sizeof(infos);
		sceNetAdhocctlGetScanInfo(&size, infos);
		for (int i = 0; i < size / (int)sizeof(AdhocctlScanInfo) && i < 8; ++i) {
			if (memcmp(infos[i].group, PAIR_GROUP, 8) == 0) {
				found = 1;
				matches++;
				printf("  Scan finds it, same BSSID as before: %s\n", memcmp(&infos[i].bssid, &originalBssid, 6) == 0 ? "yes" : "no");
			}
		}
	}
	if (!found) {
		printf("  Scan doesn't find it\n");
	}
	int start = eventCount;
	int result = sceNetAdhocctlConnect(PAIR_GROUP);
	int event = result < 0 ? -1 : waitEvent(start, 5000);
	sceNetAdhocctlGetParameter(&param);
	printf("  Connect: %08x, event %d, same BSSID as before: %s\n", result, event, memcmp(&param.bssid, &originalBssid, 6) == 0 ? "yes" : "no");
	s64 took = waitPeers(1, 10000);
	printf("  Sees the client again: %s\n", took < 0 ? "not within 10s" : "yes");
#else
	int start = eventCount;
	// Usually under 1s, but 8-16s when the leaving console's notice doesn't arrive and the peer times out.
	s64 took = waitPeers(0, 20000);
	printf("  Host gone from the peer list: %s\n", took < 0 ? "not within 20s" : "yes");
	int state = -1;
	sceNetAdhocctlGetState(&state);
	sceNetAdhocctlGetParameter(&param);
	printf("  Events: %d, state %d, same BSSID: %s\n", eventCount - start, state, memcmp(&param.bssid, &originalBssid, 6) == 0 ? "yes" : "no");

	printf("\nThe creator comes back:\n");
	took = waitPeers(1, 40000);
	printf("  Sees the host again: %s\n", took < 0 ? "not within 40s" : "yes");
#endif
	pairSync(2);

	printf("\nTwo groups with the same name:\n");
	// Both leave, then each creates the group on its own, at nearly the same time.
	adhocctlLeaveGroup();
	sceKernelDelayThread(HOST ? 2000000 : 2000000);
	adhocctlCreateGroup(PAIR_GROUP);
	sceKernelDelayThread(5000000);
	printf("  Peers after 5s: %d\n", peerCount());
	sceNetAdhocctlGetParameter(&param);
	printf("  Same BSSID as the first network: %s\n", memcmp(&param.bssid, &originalBssid, 6) == 0 ? "yes" : "no");

	pairShutdown();
	return 0;
}
