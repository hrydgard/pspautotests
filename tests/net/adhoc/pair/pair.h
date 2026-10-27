#pragma once

// Shared code for the tests that need two consoles. Each test is two programs, X_host and X_client,
// built from the same X.inc with HOST set to 1 or 0, and run at the same time on two PSPs (see
// record.py). The host creates the group, the client finds it with a scan and joins. After that the
// two move in lock step with pairSync(), so what each prints doesn't depend on timing.

#include "../adhoc.h"

#define PAIR_GROUP "PAIRTEST"
#define SYNC_PORT 0x7700

static AdhocMac peerMac;
static int syncSocket = -1;

#if !HOST
static int findGroup(AdhocctlScanInfo *found) {
	AdhocctlScanInfo infos[8];
	int size = sizeof(infos);
	if (sceNetAdhocctlGetScanInfo(&size, infos) < 0) {
		return 0;
	}
	for (int i = 0; i < size / (int)sizeof(AdhocctlScanInfo) && i < 8; ++i) {
		if (memcmp(infos[i].group, PAIR_GROUP, 8) == 0) {
			*found = infos[i];
			return 1;
		}
	}
	return 0;
}
#endif

// Host: creates the group. Client: scans until it finds the group (for up to 15 seconds), then joins it.
static int pairJoin() {
	if (handlerId < 0) {
		handlerId = sceNetAdhocctlAddHandler(&adhocctlHandler, NULL);
	}
#if HOST
	return adhocctlCreateGroup(PAIR_GROUP);
#else
	AdhocctlScanInfo info;
	int scans = 0;
	int found = 0;
	for (scans = 0; scans < 15 && !found; ++scans) {
		int start = eventCount;
		if (sceNetAdhocctlScan() < 0 || waitEvent(start, 3000) != 3) {
			sceKernelDelayThread(500000);
			continue;
		}
		found = findGroup(&info);
	}
	printf("Scan: %s\n", found ? "found the group" : "didn't find the group");
	if (!found) {
		return 0;
	}
	printf("  Scan info: channel %s, mode %d, bssid low bits %d\n",
		info.channel == 1 || info.channel == 6 || info.channel == 11 ? "1/6/11" : "unexpected", info.mode, info.bssid.data[0] & 3);
	int start = eventCount;
	int result = sceNetAdhocctlJoin(&info);
	int event = result < 0 ? -1 : waitEvent(start, 5000);
	int state = -1;
	sceNetAdhocctlGetState(&state);
	printf("Join: %08x, event %d (error %08x), state %d\n", result, event, event >= 0 ? lastError : 0, state);
	AdhocctlParameter param;
	sceNetAdhocctlGetParameter(&param);
	printf("  BSSID is the scanned one: %s\n", memcmp(&param.bssid, &info.bssid, 6) == 0 ? "yes" : "no");
	return event == 1;
#endif
}

// Waits for the other console to show up in the peer list, for up to 10 seconds.
static int pairFindPeer() {
	AdhocctlPeerInfo peers[4];
	for (int i = 0; i < 100; ++i) {
		int size = sizeof(peers);
		if (sceNetAdhocctlGetPeerList(&size, peers) >= 0 && size >= (int)sizeof(AdhocctlPeerInfo)) {
			peerMac = peers[0].mac;
			return size / (int)sizeof(AdhocctlPeerInfo);
		}
		sceKernelDelayThread(100000);
	}
	return 0;
}

// Both sides call this with the same step number, in the same order. It returns once the other side
// has reached the same step, or after 10 seconds. Returns true if it got there.
static int pairSync(int step) {
	if (syncSocket < 0) {
		syncSocket = sceNetAdhocPdpCreate(&localMac, SYNC_PORT, 0x1000, 0);
	}
	u8 msg[4] = { 'S', 'Y', 'N', (u8)step };
	int done = 0;
	for (int i = 0; i < 500 && !done; ++i) {
		sceNetAdhocPdpSend(syncSocket, &peerMac, SYNC_PORT, msg, sizeof(msg), 0, 1);
		u8 buf[16];
		int len = sizeof(buf);
		AdhocMac mac;
		u16 port;
		while (sceNetAdhocPdpRecv(syncSocket, &mac, &port, buf, &len, 0, 1) == 0) {
			if (len == 4 && buf[0] == 'S' && buf[1] == 'Y' && buf[2] == 'N' && buf[3] >= step) {
				done = 1;
			}
			len = sizeof(buf);
		}
		if (!done) {
			sceKernelDelayThread(20000);
		}
	}
	// Keep telling the other side for a little while, in case it's still waiting for this step.
	for (int i = 0; i < 5; ++i) {
		sceNetAdhocPdpSend(syncSocket, &peerMac, SYNC_PORT, msg, sizeof(msg), 0, 1);
		sceKernelDelayThread(20000);
	}
	if (!done) {
		printf("(sync %d timed out)\n", step);
	}
	return done;
}

// MAC addresses differ between consoles, so print whether a MAC is the peer's rather than the MAC.
static inline const char *pairMacName(const AdhocMac *mac) {
	if (memcmp(mac, &peerMac, sizeof(AdhocMac)) == 0)
		return "peer";
	return macName(mac);
}

static int pairStartup() {
	if (!adhocStartup()) {
		return 0;
	}
	printf("sceNetAdhocInit: %08x\n", sceNetAdhocInit());
	printf("Role: %s\n", HOST ? "host" : "client");
	if (!adhocctlStartup() || !pairJoin()) {
		return 0;
	}
	int peers = pairFindPeer();
	printf("Peers: %d\n", peers);
	return peers == 1;
}

static void pairShutdown() {
	if (syncSocket >= 0) {
		sceNetAdhocPdpDelete(syncSocket, 0);
	}
	int state = -1;
	sceNetAdhocctlGetState(&state);
	if (state != 0) {
		adhocctlLeaveGroup();
	}
	adhocShutdown();
}
