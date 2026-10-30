// Whether PTP keeps the boundaries between sends. The client sends several small messages back to back;
// the host reads them a while later, with a buffer big enough for all of them, then with one smaller than
// a message. Built as ptpmsg_host and ptpmsg_client.

#include "pair.h"

#define PTP_PORT 0x2600

static u8 buffer[4096];

#if HOST
static int ptpWaiting(int id) {
	PtpStat stats[8];
	int size = sizeof(stats);
	if (sceNetAdhocGetPtpStat(&size, stats) < 0) {
		return -1;
	}
	for (int i = 0; i < size / (int)sizeof(PtpStat) && i < 8; ++i) {
		if (stats[i].id == id) {
			return stats[i].rcv;
		}
	}
	return -2;
}

// Reads with a buffer of bufSize until nothing is left, printing each length.
static void readAll(const char *title, int id, int bufSize) {
	printf("  %s, bytes waiting %d:", title, ptpWaiting(id));
	for (int i = 0; i < 20; ++i) {
		int len = bufSize;
		int result = sceNetAdhocPtpRecv(id, buffer, &len, 0, 1);
		if (result != 0) {
			printf(" then %08x", result);
			break;
		}
		printf(" %d", len);
	}
	printf("\n");
}
#else
static int id_ = -1;

static void sendAll(const int *sizes, int count) {
	printf("  Sends:");
	for (int i = 0; i < count; ++i) {
		memset(buffer, 0x40 + i, sizes[i]);
		int len = sizes[i];
		int result = sceNetAdhocPtpSend(id_, buffer, &len, 0, 1);
		printf(" %d=%08x", sizes[i], result);
	}
	printf("\n");
}
#endif

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
#if HOST
	int listener = sceNetAdhocPtpListen(&localMac, PTP_PORT, 0x2000, 200000, 5, 1, 0);
	pairSync(1);
	int id = sceNetAdhocPtpAccept(listener, NULL, NULL, 5000000, 0);
	printf("Accept: %s\n", id > 0 ? "ok" : "failed");
	pairSync(2);
	// The client sends 100, 100, 100 and 50 bytes now.
	pairSync(3);
	sceKernelDelayThread(500000);
	printf("\nFour messages, read with a 1000-byte buffer:\n");
	readAll("Recv lengths", id, 1000);
	pairSync(4);
	// The client sends 100 and 100 bytes now.
	pairSync(5);
	sceKernelDelayThread(500000);
	printf("\nTwo 100-byte messages, read with a 60-byte buffer:\n");
	readAll("Recv lengths", id, 60);
	pairSync(6);
	sceNetAdhocPtpClose(id, 0);
	sceNetAdhocPtpClose(listener, 0);
#else
	pairSync(1);
	id_ = sceNetAdhocPtpOpen(&localMac, PTP_PORT, &peerMac, PTP_PORT, 0x2000, 200000, 5, 0);
	printf("Connect: %08x\n", sceNetAdhocPtpConnect(id_, 5000000, 0));
	pairSync(2);
	printf("\nFour messages:\n");
	static const int four[] = { 100, 100, 100, 50 };
	sendAll(four, 4);
	pairSync(3);
	pairSync(4);
	printf("\nTwo messages:\n");
	static const int two[] = { 100, 100 };
	sendAll(two, 2);
	pairSync(5);
	pairSync(6);
	sceNetAdhocPtpClose(id_, 0);
#endif
	pairShutdown();
	return 0;
}
