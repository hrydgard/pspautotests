// How many ad hoc objects can exist at once on one console, in a group of its own: PDP sockets, PTP
// sockets and matching contexts. Also which local ports port 0 picks.

#include "adhoc.h"

#define BASE_PORT 0x4000
#define MAX_TRY 300

static int ids[MAX_TRY];

static void matchingHandler(int id, int event, AdhocMac *mac, int optlen, void *opt) {
}

static const char *portRange(u16 port) {
	if (port == 0)
		return "0";
	if (port < 0x1000)
		return "below 0x1000";
	if (port < 0x8000)
		return "0x1000-0x7FFF";
	return "0x8000 or above";
}

int main(int argc, char *argv[]) {
	if (!adhocStartup()) {
		return 1;
	}
	printf("sceNetAdhocInit: %08x\n", sceNetAdhocInit());
	if (!adhocctlStartup() || !adhocctlCreateGroup("LIMITS")) {
		adhocShutdown();
		return 1;
	}

	printf("\nPDP sockets, 256 byte buffers:\n");
	int count = 0, error = 0;
	for (count = 0; count < MAX_TRY; ++count) {
		ids[count] = sceNetAdhocPdpCreate(&localMac, BASE_PORT + count, 256, 0);
		if (ids[count] <= 0) {
			error = ids[count];
			break;
		}
	}
	printf("  Created %d, then %08x\n", count, error);
	for (int i = 0; i < count; ++i) {
		sceNetAdhocPdpDelete(ids[i], 0);
	}

	printf("\nPDP sockets on port 0:\n");
	PdpStat stats[4];
	for (int i = 0; i < 3; ++i) {
		ids[i] = sceNetAdhocPdpCreate(&localMac, 0, 256, 0);
	}
	int size = sizeof(stats);
	sceNetAdhocGetPdpStat(&size, stats);
	for (int i = 0; i < size / (int)sizeof(PdpStat) && i < 3; ++i) {
		printf("  Port %s%s\n", portRange(stats[i].port), i > 0 && stats[i].port == stats[i - 1].port ? ", same as the previous one" : "");
	}
	for (int i = 0; i < 3; ++i) {
		sceNetAdhocPdpDelete(ids[i], 0);
	}

	printf("\nPTP listeners, 256 byte buffers:\n");
	for (count = 0; count < MAX_TRY; ++count) {
		ids[count] = sceNetAdhocPtpListen(&localMac, BASE_PORT + count, 256, 200000, 5, 1, 0);
		if (ids[count] <= 0) {
			error = ids[count];
			break;
		}
	}
	printf("  Created %d, then %08x\n", count, error);
	for (int i = 0; i < count; ++i) {
		sceNetAdhocPtpClose(ids[i], 0);
	}

	printf("\nPDP and PTP together:\n");
	int pdps = 0, ptps = 0;
	for (int i = 0; i < MAX_TRY; ++i) {
		int id = (i & 1) ? sceNetAdhocPtpListen(&localMac, BASE_PORT + i, 256, 200000, 5, 1, 0) : sceNetAdhocPdpCreate(&localMac, BASE_PORT + i, 256, 0);
		if (id <= 0) {
			error = id;
			break;
		}
		ids[pdps + ptps] = (i & 1) ? -id : id;
		if (i & 1) {
			ptps++;
		} else {
			pdps++;
		}
	}
	printf("  Created %d PDP and %d PTP, then %08x\n", pdps, ptps, error);
	for (int i = 0; i < pdps + ptps; ++i) {
		if (ids[i] > 0) {
			sceNetAdhocPdpDelete(ids[i], 0);
		} else {
			sceNetAdhocPtpClose(-ids[i], 0);
		}
	}

	printf("\nMatching contexts:\n");
	printf("  Init: %08x\n", sceNetAdhocMatchingInit(0x20000));
	for (count = 0; count < MAX_TRY; ++count) {
		ids[count] = sceNetAdhocMatchingCreate(1, 2, BASE_PORT + count, 256, 200000, 500000, 5, 200000, &matchingHandler);
		if (ids[count] <= 0) {
			error = ids[count];
			break;
		}
	}
	printf("  Created %d, then %08x\n", count, error);
	int started = 0;
	for (int i = 0; i < count; ++i) {
		int result = sceNetAdhocMatchingStart(ids[i], 0x30, 0x1000, 0x30, 0x1000, 0, NULL);
		if (result != 0) {
			error = result;
			break;
		}
		started++;
	}
	printf("  Started %d of them, then %08x\n", started, started < count ? error : 0);
	for (int i = 0; i < count; ++i) {
		sceNetAdhocMatchingStop(ids[i]);
		sceNetAdhocMatchingDelete(ids[i]);
	}
	printf("  Term: %08x\n", sceNetAdhocMatchingTerm());

	printf("\n");
	adhocctlLeaveGroup();
	adhocShutdown();
	return 0;
}
