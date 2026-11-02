// GetPeerList and GetScanInfo with buffers too small for one entry, and whether they touch the buffer;
// and Join while already connected. Built as listsizes_host and listsizes_client.

#include "pair.h"

static u8 raw[512];

static int untouchedFrom(int from) {
	if (from < 0) {
		from = 0;
	}
	for (int i = from; i < (int)sizeof(raw); ++i) {
		if (raw[i] != 0xCC) {
			return 0;
		}
	}
	return 1;
}

static void peerList(int size) {
	memset(raw, 0xCC, sizeof(raw));
	int len = size;
	int result = sceNetAdhocctlGetPeerList(&len, (AdhocctlPeerInfo *)raw);
	printf("  GetPeerList, size %3d: %08x, size now %d, buffer untouched: %s, past the size: %s\n", size, result, len,
		untouchedFrom(0) ? "yes" : "no", untouchedFrom(size) ? "yes" : "no");
}

#if !HOST
static void scanInfo(int size) {
	memset(raw, 0xCC, sizeof(raw));
	int len = size;
	int result = sceNetAdhocctlGetScanInfo(&len, (AdhocctlScanInfo *)raw);
	printf("  GetScanInfo, size %3d: %08x, size now %d, buffer untouched: %s, past the size: %s\n", size, result, len,
		untouchedFrom(0) ? "yes" : "no", untouchedFrom(size) ? "yes" : "no");
}
#endif

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	printf("\nOne peer, sizeof(AdhocctlPeerInfo) %d:\n", (int)sizeof(AdhocctlPeerInfo));
	int sizes[] = { 0, 1, 100, (int)sizeof(AdhocctlPeerInfo) - 1, (int)sizeof(AdhocctlPeerInfo), 200, -1 };
	for (int i = 0; i < 7; ++i) {
		peerList(sizes[i]);
	}
#if !HOST
	printf("\nOne network, sizeof(AdhocctlScanInfo) %d:\n", (int)sizeof(AdhocctlScanInfo));
	int scanSizes[] = { 0, 1, (int)sizeof(AdhocctlScanInfo) - 1, (int)sizeof(AdhocctlScanInfo), 100, -1 };
	for (int i = 0; i < 6; ++i) {
		scanInfo(scanSizes[i]);
	}

	printf("\nJoin while connected:\n");
	AdhocctlScanInfo infos[4];
	int len = sizeof(infos);
	sceNetAdhocctlGetScanInfo(&len, infos);
	int start = eventCount;
	int result = sceNetAdhocctlJoin(&infos[0]);
	int event = waitEvent(start, 2000);
	int state = -1;
	sceNetAdhocctlGetState(&state);
	printf("  Join: %08x, event %d (error %08x), state %d\n", result, event, event >= 0 ? lastError : 0, state);
#endif
	pairSync(1);
	pairShutdown();
	return 0;
}
