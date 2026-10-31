// How the time the ad hoc calls take grows with the size of what they handle: PdpSend and PdpRecv
// between two sockets of the console's own, PdpSend over the air to a MAC nobody has, PtpSend and
// PtpRecv over a connection to itself, and PollSocket by the number of sockets. Each receive is
// timed with the data already waiting. A measurement, not a regression test: the numbers differ
// from run to run.

#include "adhoc.h"

#define PORT 0x5100
#define REPEAT 20
#define BUF_SIZE 16384
#define MAX_POLL 16

static u8 sendBuf[BUF_SIZE];
static u8 recvBuf[BUF_SIZE];

static u64 timeNow() {
	return sceKernelGetSystemTimeWide();
}

static void sortU32(u32 *v, int n) {
	for (int i = 1; i < n; ++i) {
		u32 x = v[i];
		int j = i - 1;
		while (j >= 0 && v[j] > x) {
			v[j + 1] = v[j];
			--j;
		}
		v[j + 1] = x;
	}
}

static void printTimes(const char *title, u32 *us) {
	sortU32(us, REPEAT);
	printf("  %-36s min %5u med %5u us\n", title, (unsigned)us[0], (unsigned)us[REPEAT / 2]);
}

static void printRow(int size, u32 *sendUs, u32 *recvUs, int minGot, int failed) {
	sortU32(sendUs, REPEAT);
	sortU32(recvUs, REPEAT);
	printf("  %6d bytes: send min %5u med %5u us, recv min %5u med %5u us", size, (unsigned)sendUs[0], (unsigned)sendUs[REPEAT / 2], (unsigned)recvUs[0], (unsigned)recvUs[REPEAT / 2]);
	if (minGot != size) {
		printf(", got as little as %d", minGot);
	}
	if (failed) {
		printf(", %d failed", failed);
	}
	printf("\n");
}

static void timePdp() {
	static const int sizes[] = { 1, 64, 256, 1024, 2048, 4096, 8192 };
	printf("PDP, between two sockets of its own:\n");
	int a = sceNetAdhocPdpCreate(&localMac, PORT, 0x10000, 0);
	int b = sceNetAdhocPdpCreate(&localMac, PORT + 1, 0x10000, 0);
	if (a < 0 || b < 0) {
		printf("  PdpCreate: %08x %08x\n", a, b);
		return;
	}
	for (int i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); ++i) {
		int size = sizes[i];
		u32 sendUs[REPEAT], recvUs[REPEAT];
		int minGot = size, failed = 0, lastError = 0;
		for (int r = 0; r < REPEAT; ++r) {
			u64 t = timeNow();
			int result = sceNetAdhocPdpSend(a, &localMac, PORT + 1, sendBuf, size, 0, 1);
			sendUs[r] = (u32)(timeNow() - t);
			if (result < 0) {
				failed++;
				lastError = result;
			}
			sceKernelDelayThread(5000);
			AdhocPollSd sd = { b, 1, 0 };
			sceNetAdhocPollSocket(&sd, 1, 200000, 0);
			AdhocMac src;
			u16 sport;
			int len = size;
			t = timeNow();
			result = sceNetAdhocPdpRecv(b, &src, &sport, recvBuf, &len, 0, 1);
			recvUs[r] = (u32)(timeNow() - t);
			if (result < 0) {
				len = -1;
			}
			if (len < minGot) {
				minGot = len;
			}
			// Drains anything left, so the next recv gets the next send.
			do {
				len = BUF_SIZE;
			} while (sceNetAdhocPdpRecv(b, &src, &sport, recvBuf, &len, 0, 1) >= 0);
		}
		printRow(size, sendUs, recvUs, minGot, failed);
		if (failed) {
			printf("    send failed with %08x\n", lastError);
		}
	}

	printf("\nPDP, over the air to a MAC nobody has:\n");
	static const int airSizes[] = { 1, 64, 256, 1024, 1472, 2048, 4096, 8192 };
	for (int i = 0; i < (int)(sizeof(airSizes) / sizeof(airSizes[0])); ++i) {
		u32 us[REPEAT];
		int failed = 0;
		for (int r = 0; r < REPEAT; ++r) {
			u64 t = timeNow();
			if (sceNetAdhocPdpSend(a, &MAC_OTHER, PORT, sendBuf, airSizes[i], 0, 1) < 0) {
				failed++;
			}
			us[r] = (u32)(timeNow() - t);
			// One at a time, so the driver's queue is empty for each.
			sceKernelDelayThread(10000);
		}
		char title[64];
		snprintf(title, sizeof(title), "%d bytes%s", airSizes[i], failed ? " (some failed)" : "");
		printTimes(title, us);
	}
	printf("\nPDP, broadcast:\n");
	for (int i = 0; i < (int)(sizeof(airSizes) / sizeof(airSizes[0])); ++i) {
		u32 us[REPEAT];
		int failed = 0;
		for (int r = 0; r < REPEAT; ++r) {
			u64 t = timeNow();
			if (sceNetAdhocPdpSend(a, &MAC_BROADCAST, PORT + 5, sendBuf, airSizes[i], 0, 1) < 0) {
				failed++;
			}
			us[r] = (u32)(timeNow() - t);
			sceKernelDelayThread(10000);
		}
		char title[64];
		snprintf(title, sizeof(title), "%d bytes%s", airSizes[i], failed ? " (some failed)" : "");
		printTimes(title, us);
	}

	printf("\nPDP, back to back over the air:\n");
	int failed = 0, lastError = 0;
	u32 maxUs = 0;
	u64 start = timeNow();
	for (int r = 0; r < 100; ++r) {
		u64 t = timeNow();
		int result = sceNetAdhocPdpSend(a, &MAC_OTHER, PORT, sendBuf, 1024, 0, 1);
		u32 took = (u32)(timeNow() - t);
		if (took > maxUs) {
			maxUs = took;
		}
		if (result < 0) {
			failed++;
			lastError = result;
		}
	}
	printf("  100 x 1024 bytes back to back: %u us in all, slowest %u us, %d failed (%08x)\n", (unsigned)(timeNow() - start), (unsigned)maxUs, failed, lastError);
	sceKernelDelayThread(100000);

	sceNetAdhocPdpDelete(a, 0);
	sceNetAdhocPdpDelete(b, 0);
}

static void timePtp() {
	static const int sizes[] = { 1, 64, 256, 1024, 2048, 4096, 8192, 16384 };
	printf("\nPTP, connected to itself:\n");
	int l = sceNetAdhocPtpListen(&localMac, PORT + 2, 0x10000, 200000, 5, 2, 0);
	int c = sceNetAdhocPtpOpen(&localMac, PORT + 3, &localMac, PORT + 2, 0x10000, 200000, 5, 0);
	int a = sceNetAdhocPtpAccept(l, NULL, NULL, 2000000, 0);
	int conn = sceNetAdhocPtpConnect(c, 2000000, 0);
	if (l < 0 || c < 0 || a < 0 || conn < 0) {
		printf("  Connection failed: %08x %08x %08x %08x\n", l, c, a, conn);
		return;
	}
	for (int i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); ++i) {
		int size = sizes[i];
		u32 sendUs[REPEAT], recvUs[REPEAT];
		int minGot = size, failed = 0;
		for (int r = 0; r < REPEAT; ++r) {
			int len = size;
			u64 t = timeNow();
			int result = sceNetAdhocPtpSend(c, sendBuf, &len, 1000000, 0);
			sendUs[r] = (u32)(timeNow() - t);
			if (result < 0 || len != size) {
				failed++;
			}
			// Gives the stack time to deliver all of it.
			sceKernelDelayThread(20000);
			len = size;
			t = timeNow();
			result = sceNetAdhocPtpRecv(a, recvBuf, &len, 0, 1);
			recvUs[r] = (u32)(timeNow() - t);
			int got = result < 0 ? 0 : len;
			if (got < minGot) {
				minGot = got;
			}
			// Drains anything the recv left.
			while (got < size) {
				len = size - got;
				if (sceNetAdhocPtpRecv(a, recvBuf, &len, 100000, 0) < 0) {
					break;
				}
				got += len;
			}
		}
		printRow(size, sendUs, recvUs, minGot, failed);
	}

	printf("\nPTP, recv into a 16384-byte buffer:\n");
	static const int waiting[] = { 64, 4096 };
	for (int i = 0; i < 2; ++i) {
		u32 us[REPEAT];
		for (int r = 0; r < REPEAT; ++r) {
			int len = waiting[i];
			sceNetAdhocPtpSend(c, sendBuf, &len, 1000000, 0);
			sceKernelDelayThread(20000);
			len = BUF_SIZE;
			u64 t = timeNow();
			sceNetAdhocPtpRecv(a, recvBuf, &len, 0, 1);
			us[r] = (u32)(timeNow() - t);
		}
		char title[64];
		snprintf(title, sizeof(title), "%d bytes waiting", waiting[i]);
		printTimes(title, us);
	}
	sceNetAdhocPtpClose(a, 0);
	sceNetAdhocPtpClose(c, 0);
	sceNetAdhocPtpClose(l, 0);
}

static void timePoll() {
	static const int counts[] = { 1, 2, 4, 8, 16 };
	printf("\nPollSocket by number of PDP sockets, non-blocking:\n");
	int socks[MAX_POLL];
	for (int i = 0; i < MAX_POLL; ++i) {
		socks[i] = sceNetAdhocPdpCreate(&localMac, PORT + 10 + i, 0x2000, 0);
		if (socks[i] < 0) {
			printf("  PdpCreate %d: %08x\n", i, socks[i]);
			return;
		}
	}
	for (int c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); ++c) {
		int n = counts[c];
		for (int ready = 0; ready < 2; ++ready) {
			// "ready": the first socket has a datagram waiting.
			if (ready) {
				sceNetAdhocPdpSend(socks[1], &localMac, PORT + 10, sendBuf, 4, 0, 1);
				AdhocPollSd sd = { socks[0], 1, 0 };
				sceNetAdhocPollSocket(&sd, 1, 200000, 0);
			}
			u32 us[REPEAT];
			AdhocPollSd sds[MAX_POLL];
			for (int r = 0; r < REPEAT; ++r) {
				for (int i = 0; i < n; ++i) {
					sds[i].id = socks[i];
					sds[i].events = 1;
					sds[i].revents = 0;
				}
				u64 t = timeNow();
				sceNetAdhocPollSocket(sds, n, 0, 1);
				us[r] = (u32)(timeNow() - t);
			}
			char title[64];
			snprintf(title, sizeof(title), "%d sockets%s", n, ready ? ", one ready" : "");
			printTimes(title, us);
			if (ready) {
				AdhocMac src;
				u16 sport;
				int len = 16;
				sceNetAdhocPdpRecv(socks[0], &src, &sport, recvBuf, &len, 0, 1);
			}
		}
	}
	for (int i = 0; i < MAX_POLL; ++i) {
		sceNetAdhocPdpDelete(socks[i], 0);
	}
}

int main(int argc, char *argv[]) {
	if (!adhocStartup()) {
		return 1;
	}
	sceNetAdhocInit();
	if (!adhocctlStartup() || !adhocctlCreateGroup("SIZES")) {
		adhocShutdown();
		return 1;
	}
	sceKernelDelayThread(200000);
	for (int i = 0; i < BUF_SIZE; ++i) {
		sendBuf[i] = (u8)i;
	}
	printf("\n");
	timePdp();
	timePtp();
	timePoll();
	printf("\n");
	adhocctlLeaveGroup();
	adhocShutdown();
	return 0;
}
