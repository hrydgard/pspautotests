// When game mode starts and stops, between two consoles. In each round the two meet in the normal pair
// group, sync and leave it; the host calls CreateEnterGameMode at once and the client JoinEnterGameMode
// a little later (a different delay each round). Both time the GAMEMODE event from the sync, stay in
// game mode for a second, and time ExitGameMode's DISCONNECT. Not a regression test: it prints times.
// Built as gamemodetiming_host and gamemodetiming_client.

#include "pair.h"
#include "../timing.h"

#define GM_GROUP "GMTIME"

static const int joinDelaysMs[] = { 200, 1000, 3000, 5000 };

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	int rounds = sizeof(joinDelaysMs) / sizeof(joinDelaysMs[0]);
	for (int r = 0; r < rounds; ++r) {
		printf("\nRound %d, the client joining %d ms after the host:\n", r + 1, joinDelaysMs[r]);
		pairSync(10 * r + 1);
		adhocctlLeaveGroup();
		u64 start = timeNow();
#if HOST
		AdhocMac members[2] = { localMac, peerMac };
		int from = eventCount;
		u64 called = timeNow();
		int result = sceNetAdhocctlCreateEnterGameMode(GM_GROUP, 1, 2, members, 15000000, 0);
#else
		while (timeNow() - start < (u64)joinDelaysMs[r] * 1000) {
			sceKernelDelayThread(1000);
		}
		int from = eventCount;
		u64 called = timeNow();
		int result = sceNetAdhocctlJoinEnterGameMode(GM_GROUP, &peerMac, 15000000, 0);
#endif
		int event = result < 0 ? -1 : waitEvent(from, 17000);
		u64 got = timeNow();
		printf("  %s: %08x, event %d (error %08x)\n", HOST ? "CreateEnterGameMode" : "JoinEnterGameMode", result, event, event >= 0 ? lastError : 0);
		printf("  GAMEMODE %u ms after the sync, %u ms after the call\n", (unsigned)((got - start) / 1000), (unsigned)((got - called) / 1000));
		sceKernelDelayThread(1000000);
		from = eventCount;
		u64 exitCalled = timeNow();
		printf("  ExitGameMode: %08x\n", sceNetAdhocctlExitGameMode());
		event = waitEvent(from, 5000);
		printf("  Event %d %u ms after ExitGameMode\n", event, (unsigned)((timeNow() - exitCalled) / 1000));
		// Back to the pair group for the next round.
		sceKernelDelayThread(500000);
#if HOST
		adhocctlCreateGroup(PAIR_GROUP);
		sceKernelDelayThread(1000000);
#else
		sceKernelDelayThread(2500000);
		pairJoin();
#endif
		pairFindPeer();
	}
	pairShutdown();
	return 0;
}
