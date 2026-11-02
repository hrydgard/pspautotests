// What GameModeUpdateReplica writes into its info struct: on a call that brings new data, and on one
// that doesn't, and what clock its timestamp is on. The client updates its master every 100 ms; the
// host polls its replica every 5 ms with the struct filled with 0xCC first. New data comes about once a
// frame whether or not the master changed. Built as gamemodeinfo_host
// and gamemodeinfo_client.

#include "pair.h"
#include "../timing.h"

#define GM_GROUP "GMINFO"

typedef struct {
	u32 magic;
	u32 counter;
	u8 fill[16];
} GameData;

typedef struct {
	u32 length;
	u32 updated;
	u64 timestamp;
} ReplicaInfo;

static GameData master;
static GameData replica;

#if HOST
#define FILL 0xCCCCCCCCu
#define FILL64 0xCCCCCCCCCCCCCCCCull

static void describe(const char *title, const ReplicaInfo *info, u64 lastTs) {
	printf("  %s: length %s, updated %s, timestamp %s\n", title,
		info->length == FILL ? "untouched" : (info->length == 16 ? "16" : "other"),
		info->updated == FILL ? "untouched" : (info->updated == 0 ? "0" : (info->updated == 1 ? "1" : "other")),
		info->timestamp == FILL64 ? "untouched" : (info->timestamp == 0 ? "0" : (lastTs && info->timestamp == lastTs ? "the last update's" : "other")));
}

// Polls for ms milliseconds, every 5 ms.
static void poll(int rep, int ms, const char *title) {
	int updates = 0, quiet = 0, uLen16 = 0, uLenUntouched = 0;
	int qLen16 = 0, qLenUntouched = 0, qUpdated0 = 0, qTsUntouched = 0, qTsLast = 0, qTsZero = 0, qTsOther = 0;
	int futureTs = 0;
	Samples age = { 0 };
	u64 lastTs = 0;
	ReplicaInfo info;
	u64 end = timeNow() + (u64)ms * 1000;
	while (timeNow() < end) {
		memset(&info, 0xCC, sizeof(info));
		int result = sceNetAdhocGameModeUpdateReplica(rep, &info);
		u64 now = timeNow();
		if (result == 0 && info.updated == 1) {
			updates++;
			uLen16 += info.length == 16;
			uLenUntouched += info.length == FILL;
			if (info.timestamp > now) {
				futureTs++;
			} else {
				sampleAdd(&age, now - info.timestamp);
			}
			lastTs = info.timestamp;
		} else if (result == 0) {
			quiet++;
			qLen16 += info.length == 16;
			qLenUntouched += info.length == FILL;
			qUpdated0 += info.updated == 0;
			// The last update's, which is 0 before the first.
			if (info.timestamp == FILL64) {
				qTsUntouched++;
			} else if (info.timestamp == lastTs) {
				qTsLast++;
			} else if (info.timestamp == 0) {
				qTsZero++;
			} else {
				qTsOther++;
			}
		}
		sceKernelDelayThread(5000);
	}
	printf("%s:\n", title);
	printf("  Calls with new data: %s; length 16 %s, untouched %s; timestamp ahead of GetSystemTimeWide: %d\n", updates == 0 ? "none" : "some",
		uLen16 == updates ? "all" : (uLen16 ? "some" : "none"), uLenUntouched == updates ? "all" : (uLenUntouched ? "some" : "none"), futureTs);
	samplePrint("Age of the timestamp when returned", &age);
	printf("  Calls without: %s; length 16 %s, untouched %s; updated 0 %s\n", quiet == 0 ? "none" : "some",
		qLen16 == quiet ? "all" : (qLen16 ? "some" : "none"), qLenUntouched == quiet ? "all" : (qLenUntouched ? "some" : "none"),
		qUpdated0 == quiet ? "all" : (qUpdated0 ? "some" : "none"));
	printf("    timestamp untouched %s, the last update's %s, 0 %s, other %s\n",
		qTsUntouched == quiet ? "all" : (qTsUntouched ? "some" : "none"), qTsLast == quiet ? "all" : (qTsLast ? "some" : "none"),
		qTsZero == quiet ? "all" : (qTsZero ? "some" : "none"), qTsOther == quiet ? "all" : (qTsOther ? "some" : "none"));
}
#endif

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	pairSync(1);
	adhocctlLeaveGroup();

	int start = eventCount;
#if HOST
	AdhocMac members[2] = { localMac, peerMac };
	sceKernelDelayThread(500000);
	int result = sceNetAdhocctlCreateEnterGameMode(GM_GROUP, 1, 2, members, 10000000, 0);
#else
	sceKernelDelayThread(3500000);
	int result = sceNetAdhocctlJoinEnterGameMode(GM_GROUP, &peerMac, 10000000, 0);
#endif
	int event = result < 0 ? -1 : waitEvent(start, 12000);
	printf("Entering game mode: %08x, event %d\n", result, event);
	if (event != 4) {
		pairShutdown();
		return 1;
	}

	memset(&master, 0, sizeof(master));
	master.magic = HOST ? 0x484F5354 : 0x434C4E54;
	int mres = sceNetAdhocGameModeCreateMaster(&master, sizeof(master));
	int rep = sceNetAdhocGameModeCreateReplica(&peerMac, &replica, sizeof(replica));
	printf("CreateMaster: %08x\n", mres);
#if HOST
	printf("CreateReplica: %s\n", rep > 0 ? "ok" : "failed");
#endif

#if HOST
	ReplicaInfo first;
	memset(&first, 0xCC, sizeof(first));
	int r = sceNetAdhocGameModeUpdateReplica(rep, &first);
	printf("First UpdateReplica, right after creating it: %08x\n", r);
	describe("Info", &first, 0);
#endif

	// PDP doesn't get through in game mode, so no pairSync from here on: both just wait.
	sceKernelDelayThread(500000);
#if HOST
	poll(rep, 2000, "Polling every 5 ms for 2 s");
#else
	for (int i = 0; i < 20; ++i) {
		master.counter++;
		sceNetAdhocGameModeUpdateMaster();
		sceKernelDelayThread(100000);
	}
#endif
	sceKernelDelayThread(500000);

	sceNetAdhocGameModeDeleteReplica(rep);
	sceNetAdhocGameModeDeleteMaster();
	start = eventCount;
	sceNetAdhocctlExitGameMode();
	waitEvent(start, 5000);
	pairShutdown();
	return 0;
}
