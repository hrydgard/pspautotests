// Blocking socket calls: what they return and when, over a TCP connection to ourselves through the
// loopback interface. One thread blocks in accept() or recv() while the main thread checks that it
// hasn't returned yet, then does what should wake it, and waits for it with a generous timeout:
// an emulator can run emulated time far ahead of the host's network. Also SO_RCVTIMEO,
// non-blocking sockets, MSG_DONTWAIT and select() timeouts. Times are put in coarse ranges.

#include <common.h>
#include <pspkernel.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <psputility.h>
#include <psputility_netmodules.h>
#include <netinet/in.h>
#include <string.h>

#define INET_SOL_SOCKET 0xFFFF
#define INET_SO_RCVTIMEO 0x1006
#define INET_SO_NBIO 0x1009
#define INET_MSG_DONTWAIT 0x80
#define TEST_PORT 23456

// The PSP's fd_set has 256 bits. Newlib's is smaller, and select() would write past it.
typedef struct {
	u32 bits[8];
} InetFdSet;
#define INET_FD_SET(fd, set) ((set)->bits[(fd) >> 5] |= 1U << ((fd) & 31))
#define INET_FD_ISSET(fd, set) (((set)->bits[(fd) >> 5] >> ((fd) & 31)) & 1)

static int listener = -1;
static int client = -1;
static int server = -1;

static volatile int threadDone;
static volatile int threadResult;
static volatile int threadErrno;
static char recvBuf[64];

static const char *timeRange(SceInt64 us) {
	if (us < 10000)
		return "under 10 ms";
	if (us < 40000)
		return "10-40 ms";
	if (us < 100000)
		return "40-100 ms";
	return "100 ms or more";
}

static void fillAddr(struct sockaddr_in *addr) {
	memset(addr, 0, sizeof(*addr));
	addr->sin_len = sizeof(*addr);
	addr->sin_family = AF_INET;
	addr->sin_port = htons(TEST_PORT);
	addr->sin_addr.s_addr = htonl(0x7F000001);
}

static int acceptThread(SceSize args, void *argp) {
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	threadResult = sceNetInetAccept(listener, (struct sockaddr *)&addr, &len);
	threadErrno = sceNetInetGetErrno();
	threadDone = 1;
	return 0;
}

static int recvThread(SceSize args, void *argp) {
	memset(recvBuf, 0, sizeof(recvBuf));
	threadResult = sceNetInetRecv(server, recvBuf, sizeof(recvBuf) - 1, 0);
	threadErrno = sceNetInetGetErrno();
	threadDone = 1;
	return 0;
}

static SceUID startThread(SceKernelThreadEntry entry) {
	threadDone = 0;
	threadResult = 0;
	threadErrno = 0;
	// Higher priority than us, so it runs until it blocks.
	SceUID thread = sceKernelCreateThread("blocker", entry, 0x10, 0x4000, 0, NULL);
	sceKernelStartThread(thread, 0, NULL);
	return thread;
}

// Returns whether the thread finished within a second.
static int finishThread(SceUID thread) {
	SceUInt timeout = 1000000;
	if (sceKernelWaitThreadEnd(thread, &timeout) < 0) {
		sceKernelTerminateDeleteThread(thread);
		return 0;
	}
	sceKernelDeleteThread(thread);
	return 1;
}

static void testRecvNoData(const char *title, int flags) {
	SceInt64 start = sceKernelGetSystemTimeWide();
	int result = sceNetInetRecv(server, recvBuf, sizeof(recvBuf) - 1, flags);
	int err = sceNetInetGetErrno();
	printf("%s: %d, errno %d, %s\n", title, result, result < 0 ? err : 0, timeRange(sceKernelGetSystemTimeWide() - start));
}

static void testSelect(const char *title, int withServer, int sec, int usec, int showTime) {
	InetFdSet readfds;
	memset(&readfds, 0, sizeof(readfds));
	if (withServer) {
		INET_FD_SET(server, &readfds);
	}
	struct SceNetInetTimeval tv = { sec, usec };
	SceInt64 start = sceKernelGetSystemTimeWide();
	int result = sceNetInetSelect(withServer ? server + 1 : 0, (fd_set *)&readfds, NULL, NULL, &tv);
	int err = sceNetInetGetErrno();
	printf("%s: %d, errno %d, server readable %d", title, result, result < 0 ? err : 0, withServer ? (int)INET_FD_ISSET(server, &readfds) : 0);
	if (showTime) {
		printf(", %s", timeRange(sceKernelGetSystemTimeWide() - start));
	}
	printf("\n");
}

int main(int argc, char *argv[]) {
	// Flushed line by line, so a hang still shows how far it got.
	setvbuf(stdout, NULL, _IONBF, 0);
	printf("load common: %08x\n", sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON));
	printf("load inet: %08x\n", sceUtilityLoadNetModule(PSP_NET_MODULE_INET));
	int result = sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000);
	printf("sceNetInit: %08x\n", result);
	if (result < 0) {
		return 1;
	}
	result = sceNetInetInit();
	printf("sceNetInetInit: %08x\n", result);
	if (result < 0) {
		sceNetTerm();
		return 1;
	}

	struct sockaddr_in addr;
	fillAddr(&addr);
	listener = sceNetInetSocket(AF_INET, SOCK_STREAM, 0);
	printf("socket: %s\n", listener >= 0 ? "valid" : "invalid");
	printf("bind: %d\n", sceNetInetBind(listener, (struct sockaddr *)&addr, sizeof(addr)));
	printf("listen: %d\n", sceNetInetListen(listener, 1));

	SceUID thread = startThread(&acceptThread);
	printf("accept returned before a connect: %d\n", threadDone);
	client = sceNetInetSocket(AF_INET, SOCK_STREAM, 0);
	printf("connect: %d\n", sceNetInetConnect(client, (struct sockaddr *)&addr, sizeof(addr)));
	printf("accept returned after the connect: %d, socket %s\n", finishThread(thread), threadResult > 0 ? "valid" : "invalid");
	server = threadResult;
	if (server <= 0) {
		printf("accept failed, errno %d\n", threadErrno);
		goto done;
	}

	thread = startThread(&recvThread);
	printf("recv returned before a send: %d\n", threadDone);
	printf("send: %d\n", sceNetInetSend(client, "hello", 5, 0));
	int finished = finishThread(thread);
	printf("recv returned after the send: %d, %d bytes, '%s'\n", finished, threadResult, recvBuf);

	testRecvNoData("recv, MSG_DONTWAIT", INET_MSG_DONTWAIT);

	int timeoutUs = 50000;
	printf("setsockopt SO_RCVTIMEO: %d\n", sceNetInetSetsockopt(server, INET_SOL_SOCKET, INET_SO_RCVTIMEO, &timeoutUs, sizeof(timeoutUs)));
	testRecvNoData("recv, SO_RCVTIMEO 50 ms", 0);

	int on = 1;
	printf("setsockopt SO_NBIO: %d\n", sceNetInetSetsockopt(server, INET_SOL_SOCKET, INET_SO_NBIO, &on, sizeof(on)));
	testRecvNoData("recv, SO_NBIO", 0);

	testSelect("select, no sockets, 20 ms", 0, 0, 20000, 1);
	testSelect("select, nothing to read, 20 ms", 1, 0, 20000, 1);
	testSelect("select, tv_usec 1000000", 1, 0, 1000000, 1);
	sceNetInetSend(client, "again", 5, 0);
	testSelect("select, data sent, 1 s", 1, 1, 0, 0);

done:
	if (server > 0) {
		sceNetInetClose(server);
	}
	sceNetInetClose(client);
	sceNetInetClose(listener);
	sceNetInetTerm();
	sceNetTerm();
	sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
	return 0;
}
