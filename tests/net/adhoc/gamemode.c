// Game mode on one console: argument checking in CreateEnterGameMode and JoinEnterGameMode, what
// happens when the other members never turn up, and the game mode data calls outside game mode.
//
// Now and then (once in three runs, so far) the first accepted CreateEnterGameMode never delivers
// its timeout event, and adhocctl then answers BUSY to everything until the console is reset. If
// the output goes wrong from "Type 2, 5 members" on, run it again.

#include "adhoc.h"

static AdhocMac members[17];

static void printEventsSince(int start) {
	printf("    events:");
	for (int i = start; i < eventCount && i < 16; ++i) {
		printf(" %d(%08x)", eventLog[i][0], eventLog[i][1]);
	}
	printf("\n");
}

// Tries to enter game mode; if the call is accepted, waits for its event and leaves again.
static void testCreate(const char *title, const char *group, int type, int num, const AdhocMac *macs, u32 timeout) {
	int start = eventCount;
	int result = sceNetAdhocctlCreateEnterGameMode(group, type, num, macs, timeout, 0);
	printf("%s: %08x\n", title, result);
	if (result >= 0) {
		int event = waitEvent(start, 5000);
		int state = -1;
		sceNetAdhocctlGetState(&state);
		printf("    event %d, state %d\n", event, state);
		printEventsSince(start);
		start = eventCount;
		int exitResult = sceNetAdhocctlExitGameMode();
		waitEvent(start, 3000);
		sceNetAdhocctlGetState(&state);
		printf("    ExitGameMode: %08x, state %d\n", exitResult, state);
	}
}

static void testDataCalls(const char *title) {
	printf("%s:\n", title);
	u8 data[64];
	memset(data, 0x55, sizeof(data));
	printf("  CreateMaster: %08x\n", sceNetAdhocGameModeCreateMaster(data, sizeof(data)));
	printf("  UpdateMaster: %08x\n", sceNetAdhocGameModeUpdateMaster());
	printf("  DeleteMaster: %08x\n", sceNetAdhocGameModeDeleteMaster());
	int replica = sceNetAdhocGameModeCreateReplica(&MAC_OTHER, data, sizeof(data));
	printf("  CreateReplica: %08x\n", replica);
	u32 info[4] = { 16, 0, 0, 0 };
	printf("  UpdateReplica: %08x\n", sceNetAdhocGameModeUpdateReplica(replica > 0 ? replica : 1, info));
	printf("  DeleteReplica: %08x\n", sceNetAdhocGameModeDeleteReplica(replica > 0 ? replica : 1));
	struct {
		int num;
		AdhocMac macs[16];
	} gameModeInfo;
	gameModeInfo.num = 0x1234;
	printf("  GetGameModeInfo: %08x, num %x\n", sceNetAdhocctlGetGameModeInfo(&gameModeInfo), gameModeInfo.num);
}

int main(int argc, char *argv[]) {
	if (!adhocStartup()) {
		return 1;
	}
	printf("sceNetAdhocInit: %08x\n", sceNetAdhocInit());
	if (!adhocctlStartup()) {
		adhocShutdown();
		return 1;
	}
	handlerId = sceNetAdhocctlAddHandler(&adhocctlHandler, NULL);

	members[0] = localMac;
	for (int i = 1; i < 17; ++i) {
		members[i] = MAC_OTHER;
		members[i].data[5] = (u8)i;
	}

	printf("\n");
	testDataCalls("Outside game mode");

	printf("\nCreateEnterGameMode arguments:\n");
	testCreate("  NULL group", NULL, 1, 2, members, 1000000);
	testCreate("  Bad group name", "BAD-NAME", 1, 2, members, 1000000);
	testCreate("  NULL members", "GAMEMODE", 1, 2, NULL, 1000000);
	testCreate("  Type 0", "GAMEMODE", 0, 2, members, 1000000);
	testCreate("  Type 4", "GAMEMODE", 4, 2, members, 1000000);
	testCreate("  Type 1, 1 member", "GAMEMODE", 1, 1, members, 1000000);
	testCreate("  Type 1, 5 members", "GAMEMODE", 1, 5, members, 1000000);
	testCreate("  Type 2, 5 members", "GAMEMODE", 2, 5, members, 1000000);
	testCreate("  Type 3, 16 members", "GAMEMODE", 3, 16, members, 1000000);
	testCreate("  Type 3, 17 members", "GAMEMODE", 3, 17, members, 1000000);
	testCreate("  First member not local", "GAMEMODE", 1, 2, members + 1, 1000000);
	// A timeout of 0 waits for the members forever, and ExitGameMode returns BUSY meanwhile, so it isn't tested.

	printf("\nCreateEnterGameModeMin arguments:\n");
	static const int mins[][2] = { { 0, 2 }, { 1, 2 }, { 2, 2 }, { 3, 2 }, { 2, 4 }, { 4, 4 } };
	for (int i = 0; i < (int)ARRAY_SIZE(mins); ++i) {
		int start = eventCount;
		int result = sceNetAdhocctlCreateEnterGameModeMin("GAMEMODE", 1, mins[i][0], mins[i][1], members, 1000000, 0);
		printf("  min %d, num %d: %08x\n", mins[i][0], mins[i][1], result);
		if (result >= 0) {
			int event = waitEvent(start, 5000);
			printf("    event %d (%08x)\n", event, event >= 0 ? lastError : 0);
			start = eventCount;
			sceNetAdhocctlExitGameMode();
			waitEvent(start, 3000);
		}
	}

	printf("\nCreateEnterGameMode with nobody else around:\n");
	testCreate("  Type 1, 2 members, 2s", "GAMEMODE", 1, 2, members, 2000000);

	printf("\nJoinEnterGameMode:\n");
	int start = eventCount;
	printf("  NULL group: %08x\n", sceNetAdhocctlJoinEnterGameMode(NULL, &MAC_OTHER, 1000000, 0));
	printf("  NULL master: %08x\n", sceNetAdhocctlJoinEnterGameMode("GAMEMODE", NULL, 1000000, 0));
	int result = sceNetAdhocctlJoinEnterGameMode("GAMEMODE", &MAC_OTHER, 2000000, 0);
	int event = result < 0 ? -1 : waitEvent(start, 5000);
	int state = -1;
	sceNetAdhocctlGetState(&state);
	printf("  Master that isn't there, 2s: %08x, event %d, state %d\n", result, event, state);
	printEventsSince(start);
	if (state != 0) {
		start = eventCount;
		printf("  ExitGameMode: %08x\n", sceNetAdhocctlExitGameMode());
		waitEvent(start, 3000);
	}
	printf("  ExitGameMode outside game mode: %08x\n", sceNetAdhocctlExitGameMode());

	printf("\nIn a normal group:\n");
	if (adhocctlCreateGroup("NORMAL")) {
		testDataCalls("Data calls");
		testCreate("  CreateEnterGameMode", "GAMEMODE", 1, 2, members, 1000000);
		adhocctlLeaveGroup();
	}

	printf("\n");
	adhocShutdown();
	return 0;
}
