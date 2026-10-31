// How the time the sceNetInet calls take grows with the size of what they handle: send, sendto and
// sendmsg, recv, recvfrom and recvmsg by bytes (over loopback, and sends over WiFi to the gateway),
// and poll and select by the number of sockets. Each receive is timed with the data already
// waiting. A measurement, not a regression test: the numbers differ from run to run. The WiFi part
// connects with profile 1, as timing does.

#include "inet.h"

typedef struct {
	void *base;
	int len;
} InetIovec;

typedef struct {
	void *name;
	int namelen;
	InetIovec *iov;
	int iovlen;
	void *control;
	int controllen;
	int flags;
} InetMsghdr;

int sceNetInetSendmsg(int s, const InetMsghdr *msg, int flags);
int sceNetInetRecvmsg(int s, InetMsghdr *msg, int flags);
int sceNetApctlInit(int stacksize, int prio);
int sceNetApctlTerm(void);
int sceNetApctlConnect(int config);
int sceNetApctlDisconnect(void);
int sceNetApctlGetState(int *state);
int sceNetApctlGetInfo(int code, void *info);

#define PORT 24900
#define REPEAT 20
#define BUF_SIZE 16384
#define MAX_POLL 64

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

static void printRow(int size, u32 *sendUs, u32 *recvUs, int minGot) {
	sortU32(sendUs, REPEAT);
	sortU32(recvUs, REPEAT);
	printf("  %6d bytes: send min %5u med %5u us, recv min %5u med %5u us", size, (unsigned)sendUs[0], (unsigned)sendUs[REPEAT / 2], (unsigned)recvUs[0], (unsigned)recvUs[REPEAT / 2]);
	if (minGot != size) {
		printf(", got as little as %d", minGot);
	}
	printf("\n");
}

static int udpSocket(u16 port) {
	int s = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	InetAddr addr = inetMakeAddr(LOOPBACK, port);
	sceNetInetBind(s, &addr, sizeof(addr));
	return s;
}

static void timeUdp() {
	static const int sizes[] = { 1, 64, 256, 1024, 2048, 4096, 8192 };
	printf("UDP, sendto and recv:\n");
	int a = udpSocket(PORT);
	int b = udpSocket(PORT + 1);
	InetAddr addrB = inetMakeAddr(LOOPBACK, PORT + 1);
	for (int i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); ++i) {
		int size = sizes[i];
		u32 sendUs[REPEAT], recvUs[REPEAT];
		int minGot = size;
		for (int r = 0; r < REPEAT; ++r) {
			u64 t = timeNow();
			int sent = sceNetInetSendto(a, sendBuf, size, 0, &addrB, sizeof(addrB));
			sendUs[r] = (u32)(timeNow() - t);
			if (sent != size) {
				printf("  %d bytes: sendto %d, errno %d\n", size, sent, sceNetInetGetErrno());
			}
			inetWait(b, 0, 200);
			t = timeNow();
			int got = sceNetInetRecv(b, recvBuf, size, 0);
			recvUs[r] = (u32)(timeNow() - t);
			if (got < minGot) {
				minGot = got;
			}
		}
		printRow(size, sendUs, recvUs, minGot);
	}

	// An 8192-byte datagram waiting, and recv asking for less: the rest is dropped.
	printf("\nUDP, recv of part of an 8192-byte datagram:\n");
	static const int parts[] = { 64, 1024, 4096 };
	for (int i = 0; i < 3; ++i) {
		u32 us[REPEAT];
		for (int r = 0; r < REPEAT; ++r) {
			sceNetInetSendto(a, sendBuf, 8192, 0, &addrB, sizeof(addrB));
			inetWait(b, 0, 200);
			u64 t = timeNow();
			sceNetInetRecv(b, recvBuf, parts[i], 0);
			us[r] = (u32)(timeNow() - t);
		}
		char title[64];
		snprintf(title, sizeof(title), "recv %d bytes", parts[i]);
		printTimes(title, us);
	}

	printf("\nUDP, the other calls:\n");
	static const int callSizes[] = { 64, 4096 };
	for (int i = 0; i < 2; ++i) {
		int size = callSizes[i];
		u32 us[REPEAT];
		char title[64];
		InetAddr from;
		int fromLen;

		for (int r = 0; r < REPEAT; ++r) {
			sceNetInetSendto(a, sendBuf, size, 0, &addrB, sizeof(addrB));
			inetWait(b, 0, 200);
			fromLen = sizeof(from);
			u64 t = timeNow();
			sceNetInetRecvfrom(b, recvBuf, size, 0, &from, &fromLen);
			us[r] = (u32)(timeNow() - t);
		}
		snprintf(title, sizeof(title), "recvfrom %d bytes", size);
		printTimes(title, us);

		// The same bytes in one buffer and in four.
		for (int iovs = 1; iovs <= 4; iovs += 3) {
			InetIovec iov[4];
			for (int v = 0; v < iovs; ++v) {
				iov[v].base = sendBuf + v * (size / iovs);
				iov[v].len = size / iovs;
			}
			InetMsghdr msg;
			memset(&msg, 0, sizeof(msg));
			msg.name = &addrB;
			msg.namelen = sizeof(addrB);
			msg.iov = iov;
			msg.iovlen = iovs;
			u32 recvUs[REPEAT];
			for (int r = 0; r < REPEAT; ++r) {
				u64 t = timeNow();
				sceNetInetSendmsg(a, &msg, 0);
				us[r] = (u32)(timeNow() - t);
				inetWait(b, 0, 200);

				InetIovec riov[4];
				for (int v = 0; v < iovs; ++v) {
					riov[v].base = recvBuf + v * (size / iovs);
					riov[v].len = size / iovs;
				}
				InetMsghdr rmsg;
				memset(&rmsg, 0, sizeof(rmsg));
				rmsg.name = &from;
				rmsg.namelen = sizeof(from);
				rmsg.iov = riov;
				rmsg.iovlen = iovs;
				t = timeNow();
				sceNetInetRecvmsg(b, &rmsg, 0);
				recvUs[r] = (u32)(timeNow() - t);
			}
			snprintf(title, sizeof(title), "sendmsg %d bytes, %d iov", size, iovs);
			printTimes(title, us);
			snprintf(title, sizeof(title), "recvmsg %d bytes, %d iov", size, iovs);
			printTimes(title, recvUs);
		}
	}
	sceNetInetClose(a);
	sceNetInetClose(b);
}

static void timeTcp() {
	static const int sizes[] = { 1, 64, 256, 1024, 2048, 4096, 8192, 16384 };
	printf("\nTCP, send and recv:\n");
	int listener, client, server;
	if (!inetTcpPair(PORT + 2, &listener, &client, &server)) {
		printf("  TCP pair failed\n");
		return;
	}
	// The buffers default to 16384 on TCP, enough for the biggest send at once. Without TCP_NODELAY,
	// Nagle holds each send back until the delayed ACK of the one before, and recv waits ~180 ms.
	int one = 1;
	sceNetInetSetsockopt(client, INET_IPPROTO_TCP, INET_TCP_NODELAY, &one, sizeof(one));
	for (int i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); ++i) {
		int size = sizes[i];
		u32 sendUs[REPEAT], recvUs[REPEAT];
		int minGot = size;
		for (int r = 0; r < REPEAT; ++r) {
			u64 t = timeNow();
			int sent = sceNetInetSend(client, sendBuf, size, 0);
			sendUs[r] = (u32)(timeNow() - t);
			if (sent != size) {
				printf("  %d bytes: send %d, errno %d\n", size, sent, sceNetInetGetErrno());
			}
			// Gives the stack time to deliver all of it (and the client the ACKs).
			sceKernelDelayThread(20000);
			t = timeNow();
			int got = sceNetInetRecv(server, recvBuf, size, 0);
			recvUs[r] = (u32)(timeNow() - t);
			if (got < minGot) {
				minGot = got;
			}
			// Drains anything the recv left.
			while (got > 0 && got < size) {
				int more = sceNetInetRecv(server, recvBuf, size - got, INET_MSG_DONTWAIT);
				if (more <= 0) {
					break;
				}
				got += more;
			}
		}
		printRow(size, sendUs, recvUs, minGot);
	}

	printf("\nTCP, recv into a 16384-byte buffer:\n");
	static const int waiting[] = { 64, 4096 };
	for (int i = 0; i < 2; ++i) {
		u32 us[REPEAT];
		for (int r = 0; r < REPEAT; ++r) {
			sceNetInetSend(client, sendBuf, waiting[i], 0);
			sceKernelDelayThread(20000);
			u64 t = timeNow();
			sceNetInetRecv(server, recvBuf, BUF_SIZE, 0);
			us[r] = (u32)(timeNow() - t);
		}
		char title[64];
		snprintf(title, sizeof(title), "%d bytes waiting", waiting[i]);
		printTimes(title, us);
	}
	sceNetInetClose(client);
	sceNetInetClose(server);
	sceNetInetClose(listener);
}

static void timePollSelect() {
	static const int counts[] = { 1, 4, 8, 16, 32, 48, 64 };
	printf("\npoll and select by number of sockets, timeout 0:\n");
	int socks[MAX_POLL];
	for (int i = 0; i < MAX_POLL; ++i) {
		socks[i] = udpSocket(PORT + 10 + i);
	}
	int maxFd = 0;
	for (int i = 0; i < MAX_POLL; ++i) {
		if (socks[i] > maxFd) {
			maxFd = socks[i];
		}
	}
	InetAddr addr0 = inetMakeAddr(LOOPBACK, PORT + 10);
	for (int c = 0; c < (int)(sizeof(counts) / sizeof(counts[0])); ++c) {
		int n = counts[c];
		for (int ready = 0; ready < 2; ++ready) {
			// "ready": the first socket has a datagram waiting.
			if (ready) {
				sceNetInetSendto(socks[1], sendBuf, 4, 0, &addr0, sizeof(addr0));
				inetWait(socks[0], 0, 200);
			}
			InetPollFd fds[MAX_POLL];
			u32 pollUs[REPEAT], selectUs[REPEAT];
			for (int r = 0; r < REPEAT; ++r) {
				for (int i = 0; i < n; ++i) {
					fds[i].fd = socks[i];
					fds[i].events = 1;
					fds[i].revents = 0;
				}
				u64 t = timeNow();
				sceNetInetPoll(fds, n, 0);
				pollUs[r] = (u32)(timeNow() - t);

				InetFdSet set;
				memset(&set, 0, sizeof(set));
				int top = 0;
				for (int i = 0; i < n; ++i) {
					set.bits[socks[i] >> 5] |= 1U << (socks[i] & 31);
					if (socks[i] > top) {
						top = socks[i];
					}
				}
				InetTimeval tv0 = { 0, 0 };
				t = timeNow();
				sceNetInetSelect(top + 1, &set, NULL, NULL, &tv0);
				selectUs[r] = (u32)(timeNow() - t);
			}
			char title[64];
			snprintf(title, sizeof(title), "poll, %d sockets%s", n, ready ? ", one ready" : "");
			printTimes(title, pollUs);
			snprintf(title, sizeof(title), "select, %d sockets%s", n, ready ? ", one ready" : "");
			printTimes(title, selectUs);
			if (ready) {
				sceNetInetRecv(socks[0], recvBuf, 16, 0);
			}
		}
	}

	// select with nfds covering all 64, but only one socket in the set.
	u32 us[REPEAT];
	for (int r = 0; r < REPEAT; ++r) {
		InetFdSet set;
		memset(&set, 0, sizeof(set));
		set.bits[socks[0] >> 5] |= 1U << (socks[0] & 31);
		InetTimeval tv0 = { 0, 0 };
		u64 t = timeNow();
		sceNetInetSelect(maxFd + 1, &set, NULL, NULL, &tv0);
		us[r] = (u32)(timeNow() - t);
	}
	printTimes("select, 1 socket, nfds past all 64", us);

	// Whether it's the number of sockets or which ones: the last of them, alone and as a set of 16.
	for (int c = 0; c < 2; ++c) {
		int n = c == 0 ? 1 : 16;
		InetPollFd fds[16];
		for (int r = 0; r < REPEAT; ++r) {
			for (int i = 0; i < n; ++i) {
				fds[i].fd = socks[MAX_POLL - n + i];
				fds[i].events = 1;
				fds[i].revents = 0;
			}
			u64 t = timeNow();
			sceNetInetPoll(fds, n, 0);
			us[r] = (u32)(timeNow() - t);
		}
		printTimes(c == 0 ? "poll, the last socket alone" : "poll, the last 16 sockets", us);
	}
	printf("  (sockets %d to %d)\n", socks[0], socks[MAX_POLL - 1]);
	for (int c = 0; c < 2; ++c) {
		int s = socks[c == 0 ? 0 : MAX_POLL - 1];
		for (int r = 0; r < REPEAT; ++r) {
			u64 t = timeNow();
			sceNetInetRecv(s, recvBuf, 16, INET_MSG_DONTWAIT);
			us[r] = (u32)(timeNow() - t);
		}
		printTimes(c == 0 ? "recv, nothing there, the first socket" : "recv, nothing there, the last socket", us);
		int val, len;
		for (int r = 0; r < REPEAT; ++r) {
			len = sizeof(val);
			u64 t = timeNow();
			sceNetInetGetsockopt(s, INET_SOL_SOCKET, INET_SO_TYPE, &val, &len);
			us[r] = (u32)(timeNow() - t);
		}
		printTimes(c == 0 ? "getsockopt, the first socket" : "getsockopt, the last socket", us);
	}
	for (int i = 0; i < MAX_POLL; ++i) {
		sceNetInetClose(socks[i]);
	}
}

// Polls the apctl state every 1 ms until it's `state`. Returns whether it got there.
static int waitApctlState(int state, int timeoutMs) {
	u64 end = timeNow() + (u64)timeoutMs * 1000;
	while (timeNow() < end) {
		int st = -1;
		sceNetApctlGetState(&st);
		if (st == state) {
			return 1;
		}
		sceKernelDelayThread(1000);
	}
	return 0;
}

static void timeWifi() {
	static const int sizes[] = { 1, 64, 256, 1024, 1472, 2048, 4096, 8192 };
	printf("\nUDP sendto over WiFi, to the gateway's port 9:\n");
	sceNetApctlInit(0x1600, 0x30);
	sceNetApctlConnect(1);
	if (!waitApctlState(4, 15000)) {
		printf("  Didn't get an IP address within 15 s\n");
		sceNetApctlTerm();
		return;
	}
	char gateway[128];
	memset(gateway, 0, sizeof(gateway));
	sceNetApctlGetInfo(10, gateway);
	u32 ip = 0;
	sceNetInetInetAton(gateway, &ip);
	int s = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	InetAddr to;
	memset(&to, 0, sizeof(to));
	to.len = sizeof(to);
	to.family = INET_AF_INET;
	to.port = inetHtons(9);
	to.addr = ip;
	for (int i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); ++i) {
		int size = sizes[i];
		u32 us[REPEAT];
		int failed = 0;
		for (int r = 0; r < REPEAT; ++r) {
			u64 t = timeNow();
			int sent = sceNetInetSendto(s, sendBuf, size, 0, &to, sizeof(to));
			us[r] = (u32)(timeNow() - t);
			if (sent != size) {
				failed++;
			}
			// One at a time, so the driver's queue is empty for each.
			sceKernelDelayThread(10000);
		}
		char title[64];
		snprintf(title, sizeof(title), "%d bytes%s", size, failed ? " (some failed)" : "");
		printTimes(title, us);
	}

	// Back to back, 100 sends of 1024 bytes: does the driver's queue fill?
	int failed = 0, lastErrno = 0;
	u32 maxUs = 0;
	u64 start = timeNow();
	for (int r = 0; r < 100; ++r) {
		u64 t = timeNow();
		int sent = sceNetInetSendto(s, sendBuf, 1024, 0, &to, sizeof(to));
		u32 took = (u32)(timeNow() - t);
		if (took > maxUs) {
			maxUs = took;
		}
		if (sent != 1024) {
			failed++;
			lastErrno = sceNetInetGetErrno();
		}
	}
	printf("  100 x 1024 bytes back to back: %u us in all, slowest %u us, %d failed (errno %d)\n", (unsigned)(timeNow() - start), (unsigned)maxUs, failed, lastErrno);

	sceNetInetClose(s);
	sceNetApctlDisconnect();
	waitApctlState(0, 5000);
	sceNetApctlTerm();
}

int main(int argc, char *argv[]) {
	if (!inetStartup()) {
		return 1;
	}
	for (int i = 0; i < BUF_SIZE; ++i) {
		sendBuf[i] = (u8)i;
	}
	timeUdp();
	timeTcp();
	timePollSelect();
	timeWifi();
	inetShutdown();
	return 0;
}
