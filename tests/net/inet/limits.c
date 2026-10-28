// The largest socket buffer setsockopt accepts, and how receive timeouts too short for the 10 ms
// clock tick behave: a second thread sends after 300 ms, so a recv that's still waiting returns it.

#include "inet.h"
#include <pspthreadman.h>

#define PORT_A 24700
#define PORT_B 24701

static int largestAccepted(int s, int opt) {
	int lo = 65536, hi = 4 * 1024 * 1024;
	while (lo + 1 < hi) {
		int mid = lo + (hi - lo) / 2;
		if (sceNetInetSetsockopt(s, INET_SOL_SOCKET, opt, &mid, sizeof(mid)) >= 0) {
			lo = mid;
		} else {
			hi = mid;
		}
	}
	return lo;
}

static int sender;
static InetAddr addrB;

static int senderThread(SceSize args, void *argp) {
	sceKernelDelayThread(300000);
	int result = sceNetInetSendto(sender, "late", 4, 0, &addrB, sizeof(addrB));
	printf("    (sender: %d, errno %d)\n", result, result < 0 ? sceNetInetGetErrno() : 0);
	return 0;
}

static void testTimeout(const char *title, int s, const void *val, int len) {
	sceNetInetSetsockopt(s, INET_SOL_SOCKET, INET_SO_RCVTIMEO, val, len);
	SceUID thid = sceKernelCreateThread("sender", &senderThread, 0x18, 0x10000, PSP_THREAD_ATTR_USER, NULL);
	sceKernelStartThread(thid, 0, NULL);
	char buf[16];
	u64 start = sceKernelGetSystemTimeWide();
	int result = sceNetInetRecv(s, buf, sizeof(buf), 0);
	int errno_ = result < 0 ? sceNetInetGetErrno() : 0;
	int ms = (int)((sceKernelGetSystemTimeWide() - start) / 1000);
	const char *when = ms < 5 ? "under 5 ms" : (ms < 40 ? "5-40 ms" : (ms < 250 ? "40-250 ms" : "250 ms or more"));
	printf("%s: %d, errno %d, %s\n", title, result, errno_, when);
	sceKernelWaitThreadEnd(thid, NULL);
	sceKernelDeleteThread(thid);
	// Whatever the thread sent, if recv gave up first.
	sceKernelDelayThread(20000);
	sceNetInetRecv(s, buf, sizeof(buf), INET_MSG_DONTWAIT);
}

int main(int argc, char *argv[]) {
	if (!inetStartup()) {
		return 1;
	}

	printf("Largest buffer accepted:\n");
	int tcp = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
	int udp = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	printf("  TCP SO_SNDBUF: %d\n", largestAccepted(tcp, INET_SO_SNDBUF));
	printf("  TCP SO_RCVBUF: %d\n", largestAccepted(tcp, INET_SO_RCVBUF));
	printf("  UDP SO_SNDBUF: %d\n", largestAccepted(udp, INET_SO_SNDBUF));
	printf("  UDP SO_RCVBUF: %d\n", largestAccepted(udp, INET_SO_RCVBUF));
	sceNetInetClose(tcp);
	sceNetInetClose(udp);

	printf("\nReceive timeouts:\n");
	sender = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	InetAddr addrA = inetMakeAddr(LOOPBACK, PORT_A);
	sceNetInetBind(sender, &addrA, sizeof(addrA));
	int s = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	addrB = inetMakeAddr(LOOPBACK, PORT_B);
	sceNetInetBind(s, &addrB, sizeof(addrB));

	int us = 100000;
	testTimeout("  100000 us", s, &us, sizeof(us));
	us = 15000;
	testTimeout("  15000 us", s, &us, sizeof(us));
	us = 9999;
	testTimeout("  9999 us", s, &us, sizeof(us));
	us = 1;
	testTimeout("  1 us", s, &us, sizeof(us));
	InetTimeval tv = { 5, 0 };
	testTimeout("  A timeval of 5 s", s, &tv, sizeof(tv));
	us = 0;
	testTimeout("  0", s, &us, sizeof(us));

	sceNetInetClose(s);
	sceNetInetClose(sender);
	inetShutdown();
	return 0;
}
