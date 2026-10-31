// How the time sceNetInetSend/Sendto and Recv take grows with the size of the data, over loopback.
// Each receive is timed with the data already waiting. A measurement, not a regression test: the
// numbers differ from run to run.

#include "inet.h"

#define PORT 24900
#define REPEAT 20
#define BUF_SIZE 16384

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
	printf("UDP:\n");
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
	sceNetInetClose(a);
	sceNetInetClose(b);
}

static void timeTcp() {
	static const int sizes[] = { 1, 64, 256, 1024, 2048, 4096, 8192, 16384 };
	printf("\nTCP:\n");
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
	sceNetInetClose(client);
	sceNetInetClose(server);
	sceNetInetClose(listener);
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
	inetShutdown();
	return 0;
}
