// The second set of access point scan functions (MGS: Peace Walker uses them): sceNetApctlScanSSID2,
// which takes an SSID to look for (or NULL for any), and sceNetApctlGetBSSDescIDList2 and
// sceNetApctlGetBSSDescEntry2, which read its results. Like apscan, this needs the WLAN switch on and
// an access point in range, and prints only the shape of what's found.

#include "inet.h"

typedef void (*ApctlHandler)(int oldState, int newState, int event, int error, void *arg);
int sceNetApctlInit(int stacksize, int prio);
int sceNetApctlTerm(void);
int sceNetApctlGetState(int *state);
int sceNetApctlAddHandler(ApctlHandler handler, void *arg);
int sceNetApctlDelHandler(int id);
int sceNetApctlScanUser(void);
int sceNetApctlGetBSSDescIDListUser(int *size, void *buf);
int sceNetApctlGetBSSDescEntryUser(int id, int code, void *info);
int sceNetApctlScanSSID2(const char *ssid);
int sceNetApctlGetBSSDescIDList2(int *size, void *buf);
int sceNetApctlGetBSSDescEntry2(int id, int code, void *info);

#define MAX_EVENTS 64
static volatile int numEvents = 0;
static int events[MAX_EVENTS][5];
static u64 scanStart;

static const char *bucket(int ms) {
	if (ms < 100) {
		return "<0.1s";
	} else if (ms < 1000) {
		return "0.1-1s";
	} else if (ms < 3000) {
		return "1-3s";
	}
	return ">3s";
}

static void apctlHandler(int oldState, int newState, int event, int error, void *arg) {
	if (numEvents < MAX_EVENTS) {
		events[numEvents][0] = oldState;
		events[numEvents][1] = newState;
		events[numEvents][2] = event;
		events[numEvents][3] = error;
		events[numEvents][4] = (int)((sceKernelGetSystemTimeWide() - scanStart) / 1000);
		numEvents++;
	}
}

static int written(const u8 *buf, int size) {
	int last = 0;
	for (int i = 0; i < size; ++i) {
		if (buf[i] != 0xCC) {
			last = i + 1;
		}
	}
	return last;
}

static int state() {
	int s = -1;
	sceNetApctlGetState(&s);
	return s;
}

// Waits for a scan to end: an event that leaves state 0, or 8 seconds. Then a little longer, for
// anything that follows.
static void waitScan(int from) {
	for (int i = 0; i < 800; ++i) {
		int done = 0;
		for (int j = from; j < numEvents; ++j) {
			if (events[j][1] == 0) {
				done = 1;
			}
		}
		if (done && state() == 0) {
			break;
		}
		sceKernelDelayThread(10000);
	}
	sceKernelDelayThread(500000);
}

static void printEvents(int from) {
	printf("  Events:");
	if (from == numEvents) {
		printf(" none");
	}
	for (int i = from; i < numEvents; ++i) {
		printf(" %d->%d e%d(%08x) %s", events[i][0], events[i][1], events[i][2], events[i][3], bucket(events[i][4]));
	}
	printf("\n");
}

static u8 ids[2048];

// Returns how many entries the list has.
static int listShape(const char *title, int (*list)(int *, void *)) {
	int size = 0x1234;
	int r0 = list(&size, NULL);
	memset(ids, 0xCC, sizeof(ids));
	int got = sizeof(ids);
	int r1 = list(&got, ids);
	int count = r1 < 0 ? 0 : got / 8;
	// How many are found depends on what's around.
	printf("  %s: %08x, %08x, %s, %s\n", title, r0, r1, count == 0 ? "none" : "some", got == size ? "the same size" : "a different size");
	return count;
}

static void entriesShape(int count, int (*entry)(int, int, void *), const char *match) {
	int idsInOrder = 1, linked = 1, sorted = 1, lengthsMatch = 1, matching = 1;
	int lastStrength = 1000;
	for (int i = 0; i < count; ++i) {
		u32 next, id;
		memcpy(&next, ids + i * 8, 4);
		memcpy(&id, ids + i * 8 + 4, 4);
		if ((int)id != i + 1) {
			idsInOrder = 0;
		}
		if (next != (i == count - 1 ? 0 : (u32)(ids + i * 8 + 8))) {
			linked = 0;
		}
		u8 strength = 0;
		entry(id, 4, &strength);
		if (strength > lastStrength) {
			sorted = 0;
		}
		lastStrength = strength;
		char ssid[64];
		u32 length = 0;
		memset(ssid, 0, sizeof(ssid));
		entry(id, 1, ssid);
		entry(id, 2, &length);
		if (strlen(ssid) != length) {
			lengthsMatch = 0;
		}
		if (match && strcmp(ssid, match) != 0) {
			matching = 0;
		}
	}
	printf("  IDs from 1 in order: %d, linked by next: %d, strongest first: %d, SSID lengths match: %d", idsInOrder, linked, sorted, lengthsMatch);
	if (match) {
		printf(", all have the SSID: %d", matching);
	}
	printf("\n");
}

static void entryCodes(int count, int (*entry)(int, int, void *)) {
	u32 first;
	memcpy(&first, ids + 4, 4);
	for (int code = 0; code <= 6; ++code) {
		u8 info[64];
		memset(info, 0xCC, sizeof(info));
		int r = entry(first, code, info);
		printf("  Code %d: %08x, writes %d bytes\n", code, r, written(info, sizeof(info)));
	}
	static const int others[] = { 0, -1, 19, 20 };
	u8 info[64];
	printf("  ID past the last: %08x\n", entry(count + 1, 0, info));
	for (size_t i = 0; i < sizeof(others) / sizeof(others[0]); ++i) {
		printf("  ID %d: %08x\n", others[i], entry(others[i], 4, info));
	}
}

int main(int argc, char *argv[]) {
	sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
	printf("sceNetInit: %08x\n", sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000));
	printf("sceNetInetInit: %08x\n", sceNetInetInit());
	printf("sceNetApctlInit: %08x\n", sceNetApctlInit(0x1600, 0x30));
	int h = sceNetApctlAddHandler(&apctlHandler, NULL);

	printf("\nBefore any scan:\n");
	listShape("List2", sceNetApctlGetBSSDescIDList2);
	listShape("ListUser", sceNetApctlGetBSSDescIDListUser);
	printf("  List2 NULL size: %08x\n", sceNetApctlGetBSSDescIDList2(NULL, NULL));
	u8 info[64];
	printf("  Entry2 NULL info: %08x\n", sceNetApctlGetBSSDescEntry2(1, 4, NULL));
	printf("  Entry2 ID 1: %08x\n", sceNetApctlGetBSSDescEntry2(1, 4, info));
	printf("  ScanSSID2, 33 characters: %08x\n", sceNetApctlScanSSID2("123456789012345678901234567890123"));

	printf("\nScanSSID2(NULL):\n");
	int from = numEvents;
	scanStart = sceKernelGetSystemTimeWide();
	int result = sceNetApctlScanSSID2(NULL);
	printf("  %08x, state right after %d\n", result, state());
	printf("  ScanSSID2 again while scanning: %08x\n", sceNetApctlScanSSID2(NULL));
	printf("  ScanUser while scanning: %08x\n", sceNetApctlScanUser());
	waitScan(from);
	printf("  State after %d\n", state());
	printEvents(from);
	int count = listShape("List2", sceNetApctlGetBSSDescIDList2);
	char firstSsid[64] = "";
	if (count > 0) {
		entriesShape(count, sceNetApctlGetBSSDescEntry2, NULL);
		entryCodes(count, sceNetApctlGetBSSDescEntry2);
		u32 first;
		memcpy(&first, ids + 4, 4);
		sceNetApctlGetBSSDescEntry2(first, 1, firstSsid);
	}
	listShape("ListUser afterwards", sceNetApctlGetBSSDescIDListUser);

	if (firstSsid[0] != 0) {
		printf("\nScanSSID2 for the strongest one's SSID:\n");
		from = numEvents;
		scanStart = sceKernelGetSystemTimeWide();
		printf("  %08x\n", sceNetApctlScanSSID2(firstSsid));
		sceKernelDelayThread(1000000);
		listShape("List2 after 1s", sceNetApctlGetBSSDescIDList2);
		sceKernelDelayThread(4000000);
		printEvents(from);
		count = listShape("List2 after 5s", sceNetApctlGetBSSDescIDList2);
		if (count > 0) {
			entriesShape(count, sceNetApctlGetBSSDescEntry2, firstSsid);
		}
	}

	printf("\nScanSSID2 for an SSID that isn't there:\n");
	from = numEvents;
	scanStart = sceKernelGetSystemTimeWide();
	printf("  %08x\n", sceNetApctlScanSSID2("pspautotests-nowhere"));
	sceKernelDelayThread(5000000);
	printEvents(from);
	listShape("List2", sceNetApctlGetBSSDescIDList2);

	printf("\nScanUser, then both lists:\n");
	from = numEvents;
	scanStart = sceKernelGetSystemTimeWide();
	printf("  %08x\n", sceNetApctlScanUser());
	waitScan(from);
	printEvents(from);
	listShape("ListUser", sceNetApctlGetBSSDescIDListUser);
	listShape("List2", sceNetApctlGetBSSDescIDList2);

	sceNetApctlDelHandler(h);
	printf("\nsceNetApctlTerm: %08x\n", sceNetApctlTerm());
	printf("sceNetInetTerm: %08x\n", sceNetInetTerm());
	printf("sceNetTerm: %08x\n", sceNetTerm());
	sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
	return 0;
}
