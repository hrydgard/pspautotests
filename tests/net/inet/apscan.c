// What sceNetApctlScanUser's results look like: the events of a scan, the ID list, and how much each
// sceNetApctlGetBSSDescEntryUser code writes. Needs the WLAN switch on and at least one access point in
// range. What's found differs by place and time, so only its shape is printed, never names or addresses.

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

#define MAX_EVENTS 32
static volatile int numEvents = 0;
static int events[MAX_EVENTS][4];

static void apctlHandler(int oldState, int newState, int event, int error, void *arg) {
	if (numEvents < MAX_EVENTS) {
		events[numEvents][0] = oldState;
		events[numEvents][1] = newState;
		events[numEvents][2] = event;
		events[numEvents][3] = error;
		numEvents++;
	}
}

// How many bytes of a 0xCC-filled buffer were written.
static int written(const u8 *buf, int size) {
	int last = 0;
	for (int i = 0; i < size; ++i) {
		if (buf[i] != 0xCC) {
			last = i + 1;
		}
	}
	return last;
}

static void scanOnce(int n) {
	int from = numEvents;
	int result = sceNetApctlScanUser();
	int state = -1;
	for (int i = 0; i < 1500; ++i) {
		sceNetApctlGetState(&state);
		if (state == 0 && numEvents > from) {
			break;
		}
		sceKernelDelayThread(10000);
	}
	// Anything that comes after the scan completes.
	sceKernelDelayThread(500000);
	printf("Scan %d: %08x, events:", n, result);
	for (int i = from; i < numEvents; ++i) {
		printf(" %d->%d e%d(%08x)", events[i][0], events[i][1], events[i][2], events[i][3]);
	}
	printf("\n");

	static u8 ids[2048];
	memset(ids, 0xCC, sizeof(ids));
	int size = 0;
	int r0 = sceNetApctlGetBSSDescIDListUser(&size, NULL);
	int got = sizeof(ids);
	int r1 = sceNetApctlGetBSSDescIDListUser(&got, ids);
	printf("  List: %08x, %08x, %s, %s\n", r0, r1, size > 0 && size % 8 == 0 ? "some, 8 bytes each" : "none or odd",
		got == size ? "the same size" : "a different size");
	int count = got / 8;
	if (count <= 0) {
		return;
	}

	int idsInOrder = 1, linked = 1, sorted = 1, lengthsMatch = 1, strengthsInRange = 1;
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
		u8 info[64];
		memset(info, 0, sizeof(info));
		sceNetApctlGetBSSDescEntryUser(id, 4, info);
		if (info[0] > 100) {
			strengthsInRange = 0;
		}
		if (info[0] > lastStrength) {
			sorted = 0;
		}
		lastStrength = info[0];
		u8 ssid[64];
		u32 length = 0;
		memset(ssid, 0, sizeof(ssid));
		sceNetApctlGetBSSDescEntryUser(id, 1, ssid);
		sceNetApctlGetBSSDescEntryUser(id, 2, &length);
		if (strlen((const char *)ssid) != length) {
			lengthsMatch = 0;
		}
	}
	printf("  IDs from 1 in order: %d, linked by next: %d\n", idsInOrder, linked);
	printf("  Strengths 0-100: %d, strongest first: %d, SSID lengths match: %d\n", strengthsInRange, sorted, lengthsMatch);

	u32 first;
	memcpy(&first, ids + 4, 4);
	for (int code = 0; code <= 6; ++code) {
		u8 info[64];
		memset(info, 0xCC, sizeof(info));
		int r = sceNetApctlGetBSSDescEntryUser(first, code, info);
		printf("  Code %d: %08x, writes %d bytes\n", code, r, written(info, sizeof(info)));
	}
	u8 info[64];
	memset(info, 0xCC, sizeof(info));
	int r = sceNetApctlGetBSSDescEntryUser(0, 4, info);
	printf("  ID 0: %08x, writes %d bytes\n", r, written(info, sizeof(info)));
	printf("  ID past the last: %08x\n", sceNetApctlGetBSSDescEntryUser(count + 1, 0, info));
	static const int far[] = { 8, 16, 17, 18, 19, 20, 24, 28, 29, 30, 31, 1000, -1 };
	for (size_t i = 0; i < sizeof(far) / sizeof(far[0]); ++i) {
		printf("  ID %d: %08x\n", far[i], sceNetApctlGetBSSDescEntryUser(far[i], 0, info));
	}
}

int main(int argc, char *argv[]) {
	sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
	printf("sceNetInit: %08x\n", sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000));
	printf("sceNetInetInit: %08x\n", sceNetInetInit());
	printf("sceNetApctlInit: %08x\n", sceNetApctlInit(0x1600, 0x30));
	int h = sceNetApctlAddHandler(&apctlHandler, NULL);
	for (int i = 0; i < 2; ++i) {
		scanOnce(i);
	}
	sceNetApctlDelHandler(h);
	printf("sceNetApctlTerm: %08x\n", sceNetApctlTerm());
	printf("sceNetInetTerm: %08x\n", sceNetInetTerm());
	printf("sceNetTerm: %08x\n", sceNetTerm());
	sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
	return 0;
}
