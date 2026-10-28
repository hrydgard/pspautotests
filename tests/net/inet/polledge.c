// sceNetInetPoll and sceNetInetSelect at the edges: a negative fd, no events asked for, a peer that
// closed or reset the connection, and a closed descriptor in a select() set.

#include "inet.h"

#define PORT 24410

#define INET_POLLIN 0x0001
#define INET_POLLPRI 0x0002
#define INET_POLLOUT 0x0004

static void testPoll(const char *title, InetPollFd *fds, int n) {
	for (int i = 0; i < n; ++i) {
		fds[i].revents = 0x7777;
	}
	int result = sceNetInetPoll(fds, n, 0);
	printf("%s: %d, errno %d", title, result, result < 0 ? sceNetInetGetErrno() : 0);
	for (int i = 0; i < n; ++i) {
		printf(", revents %04x", (u16)fds[i].revents);
	}
	printf("\n");
}

int main(int argc, char *argv[]) {
	if (!inetStartup()) {
		return 1;
	}

	int listener, client, server;
	if (!inetTcpPair(PORT, &listener, &client, &server)) {
		printf("TCP pair failed\n");
		return 1;
	}

	InetPollFd fds[2];
	printf("poll:\n");
	fds[0].fd = client;
	fds[0].events = 0;
	testPoll("  No events", fds, 1);

	fds[0].events = INET_POLLIN | INET_POLLOUT | INET_POLLPRI;
	testPoll("  Connected, nothing to read", fds, 1);

	fds[1].fd = -1;
	fds[1].events = INET_POLLIN;
	testPoll("  With fd -1", fds, 2);

	testPoll("  nfds 0", fds, 0);

	sceNetInetClose(server);
	inetWait(client, 0, 200);
	testPoll("  The peer closed", fds, 1);
	fds[0].events = 0;
	testPoll("  The peer closed, no events", fds, 1);
	char buf[8];
	inetPrint("  recv", sceNetInetRecv(client, buf, sizeof(buf), 0));
	sceNetInetSend(client, "x", 1, 0);
	sceNetInetSend(client, "x", 1, 0);
	fds[0].events = INET_POLLIN | INET_POLLOUT;
	testPoll("  After sending into it", fds, 1);
	sceNetInetClose(client);

	if (!inetTcpPair(PORT + 1, &listener, &client, &server)) {
		printf("TCP pair failed\n");
		return 1;
	}
	sceNetInetCloseWithRST(server);
	inetWait(client, 0, 200);
	fds[0].fd = client;
	fds[0].events = INET_POLLIN | INET_POLLOUT;
	testPoll("  The peer reset", fds, 1);
	sceNetInetClose(client);

	printf("\nselect:\n");
	int udp = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	int closed = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	sceNetInetClose(closed);
	InetFdSet w;
	memset(&w, 0, sizeof(w));
	w.bits[udp >> 5] |= 1U << (udp & 31);
	w.bits[closed >> 5] |= 1U << (closed & 31);
	InetTimeval tv = { 0, 0 };
	int maxfd = udp > closed ? udp : closed;
	int result = sceNetInetSelect(maxfd + 1, NULL, &w, NULL, &tv);
	printf("  A closed fd in the set: %d, errno %d, open one %s\n", result, result < 0 ? sceNetInetGetErrno() : 0,
		(w.bits[udp >> 5] & (1U << (udp & 31))) ? "set" : "clear");
	sceNetInetClose(udp);

	sceNetInetClose(listener);
	inetShutdown();
	return 0;
}
