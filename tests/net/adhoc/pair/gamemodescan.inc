// Whether, and how, a scan sees a group in game mode, against a normal group. After a sync both leave
// the pair group. The host creates a normal group at 0.5 s and leaves it at 8 s, then creates game mode
// (with both as members, the client never joining) at 10 s. The client scans back to back for 24 s and
// prints what each scan found, by the second it started. Not a regression test: when groups come and
// go varies a little. Built as gamemodescan_host and gamemodescan_client.

#include "pair.h"

#define NORMAL_GROUP "GMSCAN1"
#define GM_GROUP "GMSCAN2"

#if !HOST
static const char *seen(const AdhocctlScanInfo *infos, int count, const char *name, int *mode) {
	for (int i = 0; i < count; ++i) {
		if (strncmp((const char *)infos[i].group, name, 8) == 0) {
			*mode = infos[i].mode;
			return "yes";
		}
	}
	return NULL;
}
#endif

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
#if HOST
	AdhocMac members[2] = { localMac, peerMac };
#endif
	pairSync(1);
	adhocctlLeaveGroup();
	u64 start = sceKernelGetSystemTimeWide();
#define AT(s) while (sceKernelGetSystemTimeWide() - start < (u64)((s) * 1000000.0)) { sceKernelDelayThread(10000); }
#if HOST
	AT(0.5);
	int from = eventCount;
	printf("Create %s: %08x\n", NORMAL_GROUP, sceNetAdhocctlCreate(NORMAL_GROUP));
	printf("  Event %d\n", waitEvent(from, 5000));
	AT(8);
	adhocctlLeaveGroup();
	AT(10);
	from = eventCount;
	printf("CreateEnterGameMode %s: %08x\n", GM_GROUP, sceNetAdhocctlCreateEnterGameMode(GM_GROUP, 1, 2, members, 12000000, 0));
	// The client never joins, so this times out (ERROR) after 12 s.
	printf("  Event %d (error %08x)\n", waitEvent(from, 15000), lastError);
	AT(25);
#else
	printf("Scans (second: what it saw):\n");
	while (sceKernelGetSystemTimeWide() - start < 24000000) {
		int second = (int)((sceKernelGetSystemTimeWide() - start) / 1000000);
		int from = eventCount;
		if (sceNetAdhocctlScan() < 0 || waitEvent(from, 3000) != 3) {
			printf("  %2d: scan failed\n", second);
			sceKernelDelayThread(200000);
			continue;
		}
		AdhocctlScanInfo infos[8];
		int size = sizeof(infos);
		sceNetAdhocctlGetScanInfo(&size, infos);
		int count = size / (int)sizeof(AdhocctlScanInfo);
		if (count > 8) {
			count = 8;
		}
		int normalMode = -1, gmMode = -1;
		const char *normal = seen(infos, count, NORMAL_GROUP, &normalMode);
		const char *gm = seen(infos, count, GM_GROUP, &gmMode);
		printf("  %2d:", second);
		if (normal) {
			printf(" %s (mode %d)", NORMAL_GROUP, normalMode);
		}
		if (gm) {
			printf(" %s (mode %d)", GM_GROUP, gmMode);
		}
		if (!normal && !gm) {
			printf(" nothing");
		}
		printf("\n");
	}
	AT(25);
#endif
	pairShutdown();
	return 0;
}
