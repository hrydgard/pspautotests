// What PollSocket reports for PTP sockets that aren't connected yet, asked for every event: a listener
// before and after a connection request arrives, and a connection before PtpConnect. Then that the
// listener still accepts and both ends still exchange data. Built as ptppoll_host and ptppoll_client.

#include "pair.h"

#define PTP_PORT 0x2500
#define ALL_EVENTS 0x1F

static u8 buffer[64];

static int pollEvents(int id, int events, int *result) {
	AdhocPollSd sd;
	sd.id = id;
	sd.events = events;
	sd.revents = 0;
	*result = sceNetAdhocPollSocket(&sd, 1, 0, 1);
	return sd.revents;
}

// Polls 20 times, 1 ms apart, and prints each answer that differs from the one before it.
static void pollRepeatedly(const char *title, int id) {
	int lastRevents = -1, lastResult = -1;
	for (int i = 0; i < 20; ++i) {
		int result;
		int revents = pollEvents(id, ALL_EVENTS, &result);
		if (revents != lastRevents || result != lastResult) {
			printf("  %s: %08x, revents %04x\n", title, result, revents);
			lastRevents = revents;
			lastResult = result;
		}
		sceKernelDelayThread(1000);
	}
}

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	memset(buffer, 0x5A, sizeof(buffer));
#if HOST
	int listener = sceNetAdhocPtpListen(&localMac, PTP_PORT, 0x2000, 200000, 5, 1, 0);
	printf("Listen: %s\n", listener > 0 ? "ok" : "failed");
	pollRepeatedly("Listener, nothing pending", listener);
	pairSync(1);
	// The client connects now. Wait for ACCEPT, polling for everything.
	int result = 0, revents = 0;
	for (int i = 0; i < 3000 && !(revents & 0x08); ++i) {
		revents = pollEvents(listener, ALL_EVENTS, &result);
		if (!(revents & 0x08)) {
			sceKernelDelayThread(1000);
		}
	}
	printf("  Listener, request pending: %08x, revents %04x\n", result, revents);
	pollRepeatedly("Listener, request pending", listener);
	int id = sceNetAdhocPtpAccept(listener, NULL, NULL, 2000000, 0);
	printf("Accept: %s\n", id > 0 ? "ok" : "failed");
	int len = 16;
	printf("Recv: %08x, len %d\n", sceNetAdhocPtpRecv(id, buffer, &len, 2000000, 0), len);
	len = 16;
	printf("Send: %08x, len %d\n", sceNetAdhocPtpSend(id, buffer, &len, 2000000, 0), len);
	pairSync(2);
	sceNetAdhocPtpClose(id, 0);
	sceNetAdhocPtpClose(listener, 0);
#else
	int id = sceNetAdhocPtpOpen(&localMac, PTP_PORT, &peerMac, PTP_PORT, 0x2000, 200000, 5, 0);
	printf("Open: %s\n", id > 0 ? "ok" : "failed");
	pollRepeatedly("Before Connect", id);
	pairSync(1);
	printf("Connect: %08x\n", sceNetAdhocPtpConnect(id, 3000000, 0));
	int len = 16;
	printf("Send: %08x, len %d\n", sceNetAdhocPtpSend(id, buffer, &len, 2000000, 0), len);
	len = 16;
	printf("Recv: %08x, len %d\n", sceNetAdhocPtpRecv(id, buffer, &len, 2000000, 0), len);
	pairSync(2);
	sceNetAdhocPtpClose(id, 0);
#endif
	pairShutdown();
	return 0;
}
