// Game mode between two consoles: they meet in a normal group to learn each other's MAC, leave it,
// then the host creates game mode with both as members and the client joins. Each publishes a master
// buffer with a counter in it and reads the other's through a replica. Built as gamemode_host and
// gamemode_client.

#include "pairmatching.h"

#define GM_GROUP "GMPAIR"
#define DATA_SIZE 64

typedef struct {
	u32 magic;
	u32 counter;
	u8 fill[DATA_SIZE - 8];
} GameData;

typedef struct {
	u32 length;
	u32 updated;
	u64 timestamp;
} ReplicaInfo;

static GameData master;
static GameData replica;

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	pairSync(1);
	adhocctlLeaveGroup();

	printf("\nEntering game mode:\n");
#if HOST
	AdhocMac members[2] = { localMac, peerMac };
#endif
	int start = eventCount;
#if HOST
	sceKernelDelayThread(500000);
	int result = sceNetAdhocctlCreateEnterGameMode(GM_GROUP, 1, 2, members, 10000000, 0);
	printf("  CreateEnterGameMode: %08x\n", result);
#else
	// The host is waiting in game mode now. Its group shows up in scans from about 2 s after it entered
	// (pair/gamemodescan), so look 3 s after.
	sceKernelDelayThread(3500000);
	int scanStart = eventCount;
	if (sceNetAdhocctlScan() >= 0 && waitEvent(scanStart, 5000) == 3) {
		AdhocctlScanInfo infos[8];
		int size = sizeof(infos);
		sceNetAdhocctlGetScanInfo(&size, infos);
		int found = 0;
		for (int i = 0; i < size / (int)sizeof(AdhocctlScanInfo) && i < 8; ++i) {
			if (memcmp(infos[i].group, GM_GROUP, 6) == 0 && infos[i].group[6] == 0) {
				printf("  Scan sees the game mode group, mode %d\n", infos[i].mode);
				found = 1;
			}
		}
		if (!found) {
			printf("  Scan doesn't see the game mode group\n");
		}
	}
	start = eventCount;
	int result = sceNetAdhocctlJoinEnterGameMode(GM_GROUP, &peerMac, 10000000, 0);
	printf("  JoinEnterGameMode: %08x\n", result);
#endif
	int event = result < 0 ? -1 : waitEvent(start, 12000);
	int state = -1;
	sceNetAdhocctlGetState(&state);
	printf("  Event %d (error %08x), state %d\n", event, event >= 0 ? lastError : 0, state);
	if (event != 4) {
		pairShutdown();
		return 1;
	}

	struct {
		int num;
		AdhocMac macs[16];
	} info;
	memset(&info, 0, sizeof(info));
	result = sceNetAdhocctlGetGameModeInfo(&info);
	printf("  GetGameModeInfo: %08x, %d members:", result, info.num);
	for (int i = 0; i < info.num && i < 16; ++i) {
		printf(" %s", pairMacName(&info.macs[i]));
	}
	printf("\n");
	int peers = 0;
	sceNetAdhocctlGetPeerList(&peers, NULL);
	printf("  Peer list: %d\n", peers / (int)sizeof(AdhocctlPeerInfo));

	printf("\nBuffers:\n");
	memset(&master, 0, sizeof(master));
	master.magic = HOST ? 0x484F5354 : 0x434C4E54;
	printf("  CreateMaster, size 0: %08x\n", sceNetAdhocGameModeCreateMaster(&master, 0));
	printf("  CreateMaster: %08x\n", sceNetAdhocGameModeCreateMaster(&master, sizeof(master)));
	printf("  CreateMaster again: %08x\n", sceNetAdhocGameModeCreateMaster(&master, sizeof(master)));
	int other = sceNetAdhocGameModeCreateReplica(&MAC_OTHER, &replica, sizeof(replica));
	printf("  CreateReplica for a non-member: %08x\n", other);
	if (other > 0) {
		sceNetAdhocGameModeDeleteReplica(other);
	}
	memset(&replica, 0, sizeof(replica));
	int rep = sceNetAdhocGameModeCreateReplica(&peerMac, &replica, sizeof(replica));
	printf("  CreateReplica: %08x\n", rep);
	printf("  CreateReplica again: %08x\n", sceNetAdhocGameModeCreateReplica(&peerMac, &replica, sizeof(replica)));

	printf("\nRunning for 3 seconds:\n");
	int updates = 0, increasing = 1, wrongMagic = 0, timestamps = 1;
	u32 lastCounter = 0;
	u64 lastTimestamp = 0;
	ReplicaInfo rinfo;
	for (int i = 0; i < 60; ++i) {
		master.counter = i + 1;
		sceNetAdhocGameModeUpdateMaster();
		memset(&rinfo, 0, sizeof(rinfo));
		rinfo.length = sizeof(rinfo);
		if (sceNetAdhocGameModeUpdateReplica(rep, &rinfo) == 0 && rinfo.updated) {
			updates++;
			if (replica.counter < lastCounter) {
				increasing = 0;
			}
			lastCounter = replica.counter;
			if (replica.magic != (HOST ? 0x434C4E54u : 0x484F5354u)) {
				wrongMagic = 1;
			}
			if (rinfo.timestamp < lastTimestamp) {
				timestamps = 0;
			}
			lastTimestamp = rinfo.timestamp;
		}
		sceKernelDelayThread(50000);
	}
	printf("  Replica updates in 60 polls: %s\n", updates == 0 ? "none" : (updates >= 40 ? "40 or more" : (updates >= 10 ? "10-39" : "1-9")));
	printf("  Peer's counter only increases: %s, magic %s, timestamps %s\n", increasing ? "yes" : "no", wrongMagic ? "wrong" : "right",
		timestamps ? "increase" : "go back");
	printf("  Info length field after the call: %d\n", (int)rinfo.length);
	printf("  UpdateReplica with NULL info: %08x\n", sceNetAdhocGameModeUpdateReplica(rep, NULL));
	printf("  UpdateReplica bad id: %08x\n", sceNetAdhocGameModeUpdateReplica(rep + 100, &rinfo));

	printf("\nLeaving:\n");
	printf("  DeleteReplica: %08x\n", sceNetAdhocGameModeDeleteReplica(rep));
	printf("  DeleteMaster: %08x\n", sceNetAdhocGameModeDeleteMaster());
	start = eventCount;
	printf("  ExitGameMode: %08x\n", sceNetAdhocctlExitGameMode());
	event = waitEvent(start, 5000);
	sceNetAdhocctlGetState(&state);
	printf("  Event %d, state %d\n", event, state);

	pairShutdown();
	return 0;
}
