// How game mode data travels between two consoles depending on how often the master changes. The host
// updates its master every 1 ms, every vblank, every two vblanks and every 50 ms, 4 s each; the client
// copies each new counter it sees from the host into its own master and updates it at once. The host
// prints how often its replica of the client changed and how long from updating a counter until the
// client's echo of it arrived. Phases are timed from GAMEMODE, which both get at the same moment
// (pair/gamemodetiming). Not a regression test: it prints times. Built as gamemodesync_host and
// gamemodesync_client.

#include "pair.h"
#include "../timing.h"
#include <pspdisplay.h>

#define GM_GROUP "GMSYNC"
#define PHASES 4
#define MAX_COUNTER 16384

typedef struct {
	u32 magic;
	u32 counter;
	u32 echo;
	u8 fill[52];
} GameData;

typedef struct {
	u32 length;
	u32 updated;
	u64 timestamp;
} ReplicaInfo;

static GameData master;
static GameData replica;
#if HOST
static u64 updatedAt[MAX_COUNTER];
#endif

static int pollReplica(int rep) {
	ReplicaInfo info;
	memset(&info, 0, sizeof(info));
	info.length = sizeof(info);
	return sceNetAdhocGameModeUpdateReplica(rep, &info) == 0 && info.updated;
}

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	pairSync(1);
	adhocctlLeaveGroup();
	int from = eventCount;
#if HOST
	AdhocMac members[2] = { localMac, peerMac };
	int result = sceNetAdhocctlCreateEnterGameMode(GM_GROUP, 1, 2, members, 15000000, 0);
#else
	sceKernelDelayThread(3500000);
	int result = sceNetAdhocctlJoinEnterGameMode(GM_GROUP, &peerMac, 15000000, 0);
#endif
	int event = result < 0 ? -1 : waitEvent(from, 17000);
	printf("Game mode: %08x, event %d\n", result, event);
	if (event != 4) {
		pairShutdown();
		return 1;
	}
	u64 start = timeNow();
	memset(&master, 0, sizeof(master));
	master.magic = 0x47414D45;
	sceNetAdhocGameModeCreateMaster(&master, sizeof(master));
	memset(&replica, 0, sizeof(replica));
	int rep = sceNetAdhocGameModeCreateReplica(&peerMac, &replica, sizeof(replica));
	printf("Buffers: %s\n", rep >= 0 ? "ok" : "failed");

#if HOST
	static const char *names[PHASES] = { "every 1 ms", "every vblank", "every two vblanks", "every 50 ms" };
	for (int p = 0; p < PHASES; ++p) {
		u64 phaseStart = start + (1 + p * 5) * 1000000ULL;
		u64 phaseEnd = phaseStart + 4000000;
		while (timeNow() < phaseStart) {
			sceKernelDelayThread(1000);
		}
		u64 begun = timeNow();
		Samples changes = { 0 }, roundTrips = { 0 };
		u64 lastChange = 0, lastUpdate = 0;
		int vblanks = 0, updates = 0;
		// Skip the echo of the last phase's last update.
		u32 lastEcho = master.counter;
		while (timeNow() < phaseEnd) {
			u64 now = timeNow();
			int due = 0;
			if (p == 0) {
				due = now - lastUpdate >= 1000;
			} else if (p == 3) {
				due = now - lastUpdate >= 50000;
			}
			if (due && master.counter + 1 < MAX_COUNTER) {
				master.counter++;
				updatedAt[master.counter] = now;
				sceNetAdhocGameModeUpdateMaster();
				lastUpdate = now;
				updates++;
			}
			if (pollReplica(rep)) {
				if (lastChange) {
					sampleAdd(&changes, now - lastChange);
				}
				lastChange = now;
				if (replica.echo != lastEcho && replica.echo < MAX_COUNTER && updatedAt[replica.echo]) {
					sampleAdd(&roundTrips, now - updatedAt[replica.echo]);
				}
				lastEcho = replica.echo;
			}
			if (p == 1 || p == 2) {
				// Update the master after each vblank (or every second one), polling meanwhile.
				int before = sceDisplayGetVcount();
				while (sceDisplayGetVcount() == before && timeNow() < phaseEnd) {
					if (pollReplica(rep)) {
						u64 t = timeNow();
						if (lastChange) {
							sampleAdd(&changes, t - lastChange);
						}
						lastChange = t;
						if (replica.echo != lastEcho && replica.echo < MAX_COUNTER && updatedAt[replica.echo]) {
							sampleAdd(&roundTrips, t - updatedAt[replica.echo]);
						}
						lastEcho = replica.echo;
					}
					sceKernelDelayThread(200);
				}
				vblanks++;
				if ((p == 1 || (vblanks & 1) == 0) && master.counter + 1 < MAX_COUNTER) {
					master.counter++;
					updatedAt[master.counter] = timeNow();
					sceNetAdhocGameModeUpdateMaster();
					updates++;
				}
			} else {
				sceKernelDelayThread(200);
			}
		}
		printf("\nMaster updated %s (%d updates, %d vblanks, %u-%u ms after GAMEMODE):\n", names[p], updates, vblanks, (unsigned)((begun - start) / 1000), (unsigned)((timeNow() - start) / 1000));
		samplePrint("Replica changes", &changes);
		samplePrint("Update -> the client's echo of it", &roundTrips);
	}
#else
	// Echo the host's counter for all the phases.
	u64 end = start + (1 + PHASES * 5) * 1000000ULL;
	int echoes = 0;
	while (timeNow() < end) {
		if (pollReplica(rep) && replica.counter != master.echo) {
			master.echo = replica.counter;
			sceNetAdhocGameModeUpdateMaster();
			echoes++;
		}
		sceKernelDelayThread(200);
	}
	printf("Echoed: %s\n", echoes > 100 ? "yes" : "no");
#endif
	sceNetAdhocGameModeDeleteReplica(rep);
	sceNetAdhocGameModeDeleteMaster();
	sceNetAdhocctlExitGameMode();
	sceKernelDelayThread(1000000);
	pairShutdown();
	return 0;
}
