// The less common sceNetInet calls: GetTcpcbstat and GetUdpcbstat (where the known fields sit in their
// entries), SocketAbort on a socket another thread is blocked on, and Sendmsg/Recvmsg.

#include "inet.h"

int sceNetInetSendmsg(int s, const void *msg, int flags);
int sceNetInetRecvmsg(int s, void *msg, int flags);

typedef struct {
	void *base;
	u32 len;
} InetIovec;

typedef struct {
	void *name;
	u32 namelen;
	InetIovec *iov;
	int iovlen;
	void *control;
	u32 controllen;
	int flags;
} InetMsghdr;

#define PORT_A 24500
#define PORT_B 24501

static u8 statBuf[4096];

// Prints the total size for 0, 1 and 2 sockets, and where the known values sit in an entry.
static void testCbstat(const char *title, int (*func)(int *, void *), int type) {
	printf("%s:\n", title);
	int size = 0x1234;
	int result = func(&size, NULL);
	printf("  No sockets, size query: %d, size %d\n", result, size);
	int a = sceNetInetSocket(INET_AF_INET, type, 0);
	InetAddr addrA = inetMakeAddr(LOOPBACK, PORT_A);
	sceNetInetBind(a, &addrA, sizeof(addrA));
	size = 0x1234;
	func(&size, NULL);
	int one = size;
	int b = sceNetInetSocket(INET_AF_INET, type, 0);
	InetAddr addrB = inetMakeAddr(LOOPBACK, PORT_B);
	sceNetInetBind(b, &addrB, sizeof(addrB));
	if (type == INET_SOCK_STREAM) {
		sceNetInetListen(b, 1);
	}
	size = 0x1234;
	func(&size, NULL);
	int two = size;
	printf("  Size with 1 socket: %d, with 2: %d\n", one, two);
	int entry = two - one;
	memset(statBuf, 0xCC, sizeof(statBuf));
	size = sizeof(statBuf);
	result = func(&size, statBuf);
	printf("  Read: %d, size %d\n", result, size);
	if (entry > 0 && entry <= 512 && size >= entry) {
		// Find the second socket's port and address in the entries.
		u16 portB = inetHtons(PORT_B);
		u32 loop = inetHtonl(LOOPBACK);
		for (int e = 0; e < size / entry && e < 4; ++e) {
			const u8 *p = statBuf + e * entry;
			printf("  Entry %d:", e);
			for (int off = 0; off + 2 <= entry; off += 2) {
				if (memcmp(p + off, &portB, 2) == 0) {
					printf(" port B at %d", off);
				}
			}
			for (int off = 0; off + 4 <= entry; off += 4) {
				if (memcmp(p + off, &loop, 4) == 0) {
					printf(" 127.0.0.1 at %d", off);
				}
			}
			u32 next;
			memcpy(&next, p, 4);
			printf(", first word %s\n", next == 0 ? "0" : ((u8 *)next >= statBuf && (u8 *)next < statBuf + size ? "points into the buffer" : "other"));
		}
	}
	size = 4;
	result = func(&size, statBuf);
	printf("  Buffer of 4: %d, size %d\n", result, size);
	sceNetInetClose(a);
	sceNetInetClose(b);
}

static volatile int threadResult;
static volatile int threadErrno;
static volatile int threadDone;

static int recvThread(SceSize argc, void *argv) {
	u8 buf[32];
	threadResult = sceNetInetRecv(*(int *)argv, buf, sizeof(buf), 0);
	threadErrno = sceNetInetGetErrno();
	threadDone = 1;
	return 0;
}

static void testAbort() {
	printf("\nSocketAbort:\n");
	int s = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	InetAddr addr = inetMakeAddr(LOOPBACK, PORT_A + 10);
	sceNetInetBind(s, &addr, sizeof(addr));
	// A receive timeout, so a call that isn't aborted still comes back.
	u32 timeout = 2000000;
	sceNetInetSetsockopt(s, INET_SOL_SOCKET, INET_SO_RCVTIMEO, &timeout, sizeof(timeout));
	threadDone = 0;
	SceUID thread = sceKernelCreateThread("recv", &recvThread, 0x18, 0x4000, 0, NULL);
	sceKernelStartThread(thread, sizeof(s), &s);
	sceKernelDelayThread(100000);
	int before = threadDone;
	inetPrint("  SocketAbort", sceNetInetSocketAbort(s));
	for (int i = 0; i < 50 && !threadDone; ++i) {
		sceKernelDelayThread(10000);
	}
	printf("  Blocked recv: done before %d, within 0.5s %d, result %d, errno %d\n", before, threadDone, threadResult, threadErrno);
	sceKernelWaitThreadEnd(thread, NULL);
	sceKernelDeleteThread(thread);
	u8 buf[8];
	inetPrint("  recv afterwards, DONTWAIT", sceNetInetRecv(s, buf, sizeof(buf), INET_MSG_DONTWAIT));
	inetPrint("  sendto afterwards", sceNetInetSendto(s, "x", 1, 0, &addr, sizeof(addr)));
	inetPrint("  SocketAbort on a bad socket", sceNetInetSocketAbort(200));
	sceNetInetClose(s);
}

static void testMsg() {
	printf("\nSendmsg / Recvmsg:\n");
	int a = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	int b = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	InetAddr addrA = inetMakeAddr(LOOPBACK, PORT_A + 20);
	InetAddr addrB = inetMakeAddr(LOOPBACK, PORT_B + 20);
	sceNetInetBind(a, &addrA, sizeof(addrA));
	sceNetInetBind(b, &addrB, sizeof(addrB));

	InetIovec out[2] = { { (void *)"hello ", 6 }, { (void *)"world", 6 } };
	InetMsghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.name = &addrB;
	msg.namelen = sizeof(addrB);
	msg.iov = out;
	msg.iovlen = 2;
	inetPrint("  sendmsg, two parts", sceNetInetSendmsg(a, &msg, 0));

	char part1[8], part2[16];
	memset(part1, 0, sizeof(part1));
	memset(part2, 0, sizeof(part2));
	InetIovec in[2] = { { part1, 4 }, { part2, 15 } };
	InetAddr from;
	memset(&from, 0xCC, sizeof(from));
	InetMsghdr rmsg;
	memset(&rmsg, 0, sizeof(rmsg));
	rmsg.name = &from;
	rmsg.namelen = sizeof(from);
	rmsg.iov = in;
	rmsg.iovlen = 2;
	inetWait(b, 0, 1000);
	int result = sceNetInetRecvmsg(b, &rmsg, 0);
	printf("  recvmsg: %d, '%.4s' + '%s', from port %d, namelen %d, flags %x\n", result, part1, part2, inetHtons(from.port), (int)rmsg.namelen, rmsg.flags);

	// A datagram bigger than the buffers: the rest is dropped, and flags should say so.
	msg.iovlen = 1;
	inetPrint("  sendmsg, one part", sceNetInetSendmsg(a, &msg, 0));
	memset(part2, 0, sizeof(part2));
	InetIovec small = { part2, 3 };
	rmsg.namelen = sizeof(from);
	rmsg.iov = &small;
	rmsg.iovlen = 1;
	rmsg.flags = 0;
	inetWait(b, 0, 1000);
	result = sceNetInetRecvmsg(b, &rmsg, 0);
	printf("  recvmsg into 3: %d, '%s', flags %x\n", result, part2, rmsg.flags);

	// No name: the address isn't needed on a received message.
	sceNetInetSendmsg(a, &msg, 0);
	rmsg.name = NULL;
	rmsg.namelen = 0;
	rmsg.iov = in;
	rmsg.iovlen = 2;
	rmsg.flags = 0;
	inetWait(b, 0, 1000);
	inetPrint("  recvmsg, no name", sceNetInetRecvmsg(b, &rmsg, 0));
	inetPrint("  recvmsg, nothing waiting, DONTWAIT", sceNetInetRecvmsg(b, &rmsg, INET_MSG_DONTWAIT));
	inetPrint("  sendmsg on a bad socket", sceNetInetSendmsg(200, &msg, 0));
	msg.name = NULL;
	msg.namelen = 0;
	inetPrint("  sendmsg, no destination, unconnected", sceNetInetSendmsg(a, &msg, 0));
	sceNetInetClose(a);
	sceNetInetClose(b);
}

int main(int argc, char *argv[]) {
	if (!inetStartup()) {
		return 1;
	}
	testCbstat("GetUdpcbstat", sceNetInetGetUdpcbstat, INET_SOCK_DGRAM);
	printf("\n");
	testCbstat("GetTcpcbstat", sceNetInetGetTcpcbstat, INET_SOCK_STREAM);
	testAbort();
	testMsg();
	inetShutdown();
	return 0;
}
