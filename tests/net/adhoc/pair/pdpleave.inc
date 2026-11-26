// What a PDP socket does when the group goes away under it (pair/ptpclose and pair/vanish ask this of
// PTP). First the client leaves the group while the host is blocked in a PdpRecv with a 3 s timeout,
// and joins again. Then the client blocks in a PdpRecv itself while another of its threads leaves the
// group, and tries the socket outside the group. Built as pdpleave_host and pdpleave_client.

// Define SHOW_TIMES to also print the times, which differ from run to run.
#include "pair.h"

#define DATA_PORT 0x7713

static int sock = -1;

// A blocking receive with a 3 s timeout: what it returned, and whether it took the whole timeout.
static void blockedRecv(const char *title) {
	u8 buf[16];
	int len = sizeof(buf);
	AdhocMac mac;
	u16 port;
	u64 begin = sceKernelGetSystemTimeWide();
	int result = sceNetAdhocPdpRecv(sock, &mac, &port, buf, &len, 3000000, 0);
	u64 took = sceKernelGetSystemTimeWide() - begin;
	printf("%s: %08x, %s\n", title, result, took >= 2900000 ? "at the timeout" : "before the timeout");
#ifdef SHOW_TIMES
	printf("[times] after %llu us\n", took);
#endif
}

static void trySocket(const char *when) {
	u8 buf[16];
	int len = sizeof(buf);
	AdhocMac mac;
	u16 port;
	printf("%s:\n", when);
	printf("  Recv, non-blocking: %08x\n", sceNetAdhocPdpRecv(sock, &mac, &port, buf, &len, 0, 1));
	printf("  Send to the peer: %08x\n", sceNetAdhocPdpSend(sock, &peerMac, DATA_PORT, "DATA", 4, 0, 1));
	printf("  Broadcast: %08x\n", sceNetAdhocPdpSend(sock, &MAC_BROADCAST, DATA_PORT, "DATA", 4, 0, 1));
	AdhocPollSd sd = { sock, 0x1F, 0xCCCC };
	int result = sceNetAdhocPollSocket(&sd, 1, 0, 1);
	printf("  Poll: %08x, revents %04x\n", result, sd.revents);
	PdpStat stats[4];
	int size = sizeof(stats);
	result = sceNetAdhocGetPdpStat(&size, stats);
	printf("  GetPdpStat: %08x, %d socket(s)\n", result, size / (int)sizeof(PdpStat));
}

#if !HOST
static int leaveThread(SceSize args, void *argp) {
	sceKernelDelayThread(500000);
	sceNetAdhocctlDisconnect();
	return 0;
}
#endif

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	pairSync(10);
	sock = sceNetAdhocPdpCreate(&localMac, DATA_PORT, 0x2000, 0);
	printf("PdpCreate: %s\n", sock > 0 ? "ok" : "failed");
	pairSync(11);
#if HOST
	printf("\nThe client leaves:\n");
	blockedRecv("Blocked recv meanwhile");
	trySocket("With the client gone");
	// The client is back after 5 s.
	sceKernelDelayThread(2500000);
	printf("Client back: %s\n", pairFindPeer() == 1 ? "yes" : "no");
	pairSync(12);
	// The client leaves for good.
	sceKernelDelayThread(6000000);
#else
	sceKernelDelayThread(500000);
	adhocctlLeaveGroup();
	sceKernelDelayThread(4500000);
	printf("Back in the group: %s\n", pairJoin() && pairFindPeer() == 1 ? "yes" : "no");
	pairSync(12);
	printf("\nLeaving the group from another thread:\n");
	SceUID thread = sceKernelCreateThread("leave", &leaveThread, 0x18, 0x4000, 0, NULL);
	sceKernelStartThread(thread, 0, NULL);
	blockedRecv("Blocked recv meanwhile");
	sceKernelWaitThreadEnd(thread, NULL);
	sceKernelDeleteThread(thread);
	int state = -1;
	sceNetAdhocctlGetState(&state);
	printf("adhocctl state: %d\n", state);
	trySocket("Outside the group");
	int second = sceNetAdhocPdpCreate(&localMac, DATA_PORT + 1, 0x2000, 0);
	if (second > 0) {
		printf("PdpCreate outside the group: ok\n");
	} else {
		printf("PdpCreate outside the group: %08x\n", second);
	}
	if (second > 0) {
		sceNetAdhocPdpDelete(second, 0);
	}
#endif
	printf("PdpDelete: %08x\n", sceNetAdhocPdpDelete(sock, 0));
	pairShutdown();
	return 0;
}
