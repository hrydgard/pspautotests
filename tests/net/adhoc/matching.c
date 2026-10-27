// sceNetAdhocMatching on one console, in a group of its own: Init, argument checking in Create,
// and the calls on contexts that aren't running.

#include "adhoc.h"

#define PORT 0x3456

static void matchingHandler(int id, int event, AdhocMac *mac, int optlen, void *opt) {
}

static int lastId = 0;

static void printId(const char *title, int result) {
	if (result > 0) {
		if (lastId == 0) {
			printf("%s: context\n", title);
		} else {
			printf("%s: context, id +%d\n", title, result - lastId);
		}
		lastId = result;
	} else {
		printf("%s: %08x\n", title, result);
	}
}

static void testCreate(const char *title, int mode, int maxnum, int port, int rxbuflen, u32 hello, u32 keepalive, int count, u32 rexmt, MatchingHandler handler) {
	int result = sceNetAdhocMatchingCreate(mode, maxnum, port, rxbuflen, hello, keepalive, count, rexmt, handler);
	printId(title, result);
	if (result > 0) {
		sceNetAdhocMatchingDelete(result);
	}
}

static void testModes(const char *title, int maxnum, int rxbuflen, u32 hello, u32 keepalive, int count, u32 rexmt) {
	static const char *modes[] = { "host", "client", "p2p" };
	for (int mode = 1; mode <= 3; ++mode) {
		char full[128];
		snprintf(full, sizeof(full), "%s, %s", title, modes[mode - 1]);
		testCreate(full, mode, maxnum, PORT, rxbuflen, hello, keepalive, count, rexmt, &matchingHandler);
	}
}

int main(int argc, char *argv[]) {
	if (!adhocStartup()) {
		return 1;
	}
	printf("sceNetAdhocInit: %08x\n", sceNetAdhocInit());
	if (!adhocctlStartup() || !adhocctlCreateGroup("MATCHTST")) {
		adhocShutdown();
		return 1;
	}

	printf("\nInit:\n");
	testCreate("  Create before init", 1, 4, PORT, 1024, 200000, 200000, 5, 200000, &matchingHandler);
	printf("  Term before init: %08x\n", sceNetAdhocMatchingTerm());
	printf("  Init: %08x\n", sceNetAdhocMatchingInit(0x20000));
	printf("  Init again: %08x\n", sceNetAdhocMatchingInit(0x20000));
	printf("  GetPoolMaxAlloc: %s\n", sceNetAdhocMatchingGetPoolMaxAlloc() > 0 ? "positive" : "not positive");
	int stat[3] = { -1, -1, -1 };
	int result = sceNetAdhocMatchingGetPoolStat(stat);
	printf("  GetPoolStat: %08x, pool %x, max %s, free %s\n", result, stat[0], stat[1] > 0 && stat[1] <= stat[0] ? "ok" : "odd", stat[2] > 0 && stat[2] <= stat[0] ? "ok" : "odd");
	printf("  GetPoolStat NULL: %08x\n", sceNetAdhocMatchingGetPoolStat(NULL));

	printf("\nCreate:\n");
	testModes("  Normal", 4, 1024, 200000, 200000, 5, 200000);
	testCreate("  Mode 0", 0, 4, PORT, 1024, 200000, 200000, 5, 200000, &matchingHandler);
	testCreate("  Mode 4", 4, 4, PORT, 1024, 200000, 200000, 5, 200000, &matchingHandler);
	testModes("  Maxnum 1", 1, 1024, 200000, 200000, 5, 200000);
	testModes("  Maxnum 2", 2, 1024, 200000, 200000, 5, 200000);
	testModes("  Maxnum 16", 16, 1024, 200000, 200000, 5, 200000);
	testModes("  Maxnum 17", 17, 1024, 200000, 200000, 5, 200000);
	testModes("  Rxbuflen 0", 4, 0, 200000, 200000, 5, 200000);
	testModes("  Rxbuflen 27 (maxnum 4)", 4, 27, 200000, 200000, 5, 200000);
	testModes("  Rxbuflen 28 (maxnum 4)", 4, 28, 200000, 200000, 5, 200000);
	testModes("  Rxbuflen 15 (maxnum 2)", 2, 15, 200000, 200000, 5, 200000);
	testModes("  Rxbuflen 16 (maxnum 2)", 2, 16, 200000, 200000, 5, 200000);
	testModes("  Hello 0", 4, 1024, 0, 200000, 5, 200000);
	testModes("  Keepalive 0", 4, 1024, 200000, 0, 5, 200000);
	testModes("  Count 0", 4, 1024, 200000, 200000, 0, 200000);
	testModes("  Count -1", 4, 1024, 200000, 200000, -1, 200000);
	testModes("  Rexmt 0", 4, 1024, 200000, 200000, 5, 0);
	testCreate("  Port 0", 1, 4, 0, 1024, 200000, 200000, 5, 200000, &matchingHandler);
	testCreate("  NULL handler", 1, 4, PORT, 1024, 200000, 200000, 5, 200000, NULL);

	int id = sceNetAdhocMatchingCreate(1, 4, PORT, 1024, 200000, 200000, 5, 200000, &matchingHandler);
	printId("  Context", id);
	testCreate("  Same port again", 1, 4, PORT, 1024, 200000, 200000, 5, 200000, &matchingHandler);

	printf("\nNot started:\n");
	printf("  Stop: %08x\n", sceNetAdhocMatchingStop(id));
	printf("  SelectTarget: %08x\n", sceNetAdhocMatchingSelectTarget(id, &MAC_OTHER, 0, NULL));
	printf("  CancelTarget: %08x\n", sceNetAdhocMatchingCancelTarget(id, &MAC_OTHER));
	printf("  SendData: %08x\n", sceNetAdhocMatchingSendData(id, &MAC_OTHER, 4, "data"));
	printf("  SetHelloOpt: %08x\n", sceNetAdhocMatchingSetHelloOpt(id, 4, "data"));
	int optlen = 0x1234;
	char opt[16];
	printf("  GetHelloOpt: %08x, len %x\n", sceNetAdhocMatchingGetHelloOpt(id, &optlen, opt), optlen);
	int size = 0x1234;
	printf("  GetMembers: %08x, size %x\n", sceNetAdhocMatchingGetMembers(id, &size, NULL), size);
	printf("  Invalid id Stop: %08x\n", sceNetAdhocMatchingStop(100));
	printf("  Invalid id Delete: %08x\n", sceNetAdhocMatchingDelete(100));
	printf("  Invalid id GetMembers: %08x\n", sceNetAdhocMatchingGetMembers(100, &size, NULL));

	printf("\nStarted:\n");
	printf("  Start: %08x\n", sceNetAdhocMatchingStart(id, 0x30, 0x2000, 0x30, 0x2000, 4, "helo"));
	printf("  Start again: %08x\n", sceNetAdhocMatchingStart(id, 0x30, 0x2000, 0x30, 0x2000, 0, NULL));
	// Start2 adds a memory partition for each thread, after its stack size; the swapped order is tried too.
	static const int start2Args[][6] = {
		{ 0x30, 2, 0x2000, 0x30, 2, 0x2000 },
		{ 0x30, 2, 0x4000, 0x30, 2, 0x4000 },
		{ 0x30, 0x2000, 2, 0x30, 0x2000, 2 },
		{ 0x30, 0x4000, 2, 0x30, 0x4000, 2 },
		{ 0x30, 9, 0x4000, 0x30, 9, 0x4000 },
		{ 0x30, 0x4000, 9, 0x30, 0x4000, 9 },
	};
	for (size_t i = 0; i < sizeof(start2Args) / sizeof(start2Args[0]); ++i) {
		const int *a = start2Args[i];
		int id2 = sceNetAdhocMatchingCreate(1, 4, PORT + 1, 1024, 200000, 200000, 5, 200000, &matchingHandler);
		int result = sceNetAdhocMatchingStart2(id2, a[0], a[1], a[2], a[3], a[4], a[5], 0, NULL);
		printf("  Start2 %x, %x, %x: %08x", a[0], a[1], a[2], result);
		if (result == 0) {
			printf(", again: %08x", sceNetAdhocMatchingStart2(id2, a[0], a[1], a[2], a[3], a[4], a[5], 0, NULL));
		}
		printf("\n");
		sceNetAdhocMatchingStop(id2);
		sceNetAdhocMatchingDelete(id2);
	}
	optlen = sizeof(opt);
	memset(opt, 0, sizeof(opt));
	printf("  GetHelloOpt: %08x, len %d, '%s'\n", sceNetAdhocMatchingGetHelloOpt(id, &optlen, opt), optlen, opt);
	printf("  SetHelloOpt: %08x\n", sceNetAdhocMatchingSetHelloOpt(id, 6, "hello!"));
	optlen = 2;
	memset(opt, 0, sizeof(opt));
	printf("  GetHelloOpt into 2: %08x, len %d, '%s'\n", sceNetAdhocMatchingGetHelloOpt(id, &optlen, opt), optlen, opt);
	size = 0x1234;
	printf("  GetMembers size: %08x, size %d\n", sceNetAdhocMatchingGetMembers(id, &size, NULL), size);
	printf("  SelectTarget unknown: %08x\n", sceNetAdhocMatchingSelectTarget(id, &MAC_OTHER, 0, NULL));
	printf("  CancelTarget unknown: %08x\n", sceNetAdhocMatchingCancelTarget(id, &MAC_OTHER));
	printf("  SendData unknown: %08x\n", sceNetAdhocMatchingSendData(id, &MAC_OTHER, 4, "data"));
	printf("  Delete while running: %08x\n", sceNetAdhocMatchingDelete(id));
	printf("  Stop: %08x\n", sceNetAdhocMatchingStop(id));
	printf("  Stop again: %08x\n", sceNetAdhocMatchingStop(id));
	printf("  Delete: %08x\n", sceNetAdhocMatchingDelete(id));
	printf("  Delete again: %08x\n", sceNetAdhocMatchingDelete(id));

	printf("\nTerm:\n");
	printf("  Term: %08x\n", sceNetAdhocMatchingTerm());
	printf("  Term again: %08x\n", sceNetAdhocMatchingTerm());

	printf("\n");
	adhocctlLeaveGroup();
	adhocShutdown();
	return 0;
}
