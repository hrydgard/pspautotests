// TCP over loopback: stream boundaries, MSG_PEEK and MSG_WAITALL, half-closing with shutdown,
// non-blocking connect, the listen backlog, the addresses accept and getpeername report, and how
// much a connection buffers before send blocks.

#include "inet.h"

static int port = 24400;
static u8 buf[0x10000];

static void fill(int len, u8 seed) {
	for (int i = 0; i < len; ++i) {
		buf[i] = (u8)(i + seed);
	}
}

static int recvPrint(const char *title, int s, int len, int flags) {
	memset(buf, 0, len);
	int ready = inetWait(s, 0, 1000);
	int result = sceNetInetRecv(s, buf, len, flags);
	if (result < 0) {
		printf("%s: %d, ready %d, errno %d\n", title, result, ready, sceNetInetGetErrno());
	} else {
		printf("%s: %d, first byte %02x\n", title, result, buf[0]);
	}
	return result;
}

static const char *sizeBucket(int bytes) {
	if (bytes < 4096)
		return "under 4K";
	if (bytes < 16384)
		return "4K-16K";
	if (bytes < 65536)
		return "16K-64K";
	if (bytes < 262144)
		return "64K-256K";
	return "256K or more";
}

int main(int argc, char *argv[]) {
	if (!inetStartup()) {
		return 1;
	}
	int listener, client, server;

	printf("Stream:\n");
	if (inetTcpPair(port++, &listener, &client, &server)) {
		fill(10, 1);
		sceNetInetSend(client, buf, 10, 0);
		fill(10, 2);
		sceNetInetSend(client, buf, 10, 0);
		sceKernelDelayThread(20000);
		recvPrint("  Two sends of 10, recv 64", server, 64, 0);
		fill(30, 3);
		sceNetInetSend(client, buf, 30, 0);
		recvPrint("  Peek 64", server, 64, INET_MSG_PEEK);
		recvPrint("  Recv 10", server, 10, 0);
		recvPrint("  Recv 64", server, 64, 0);
		inetPrint("  Recv with nothing there, DONTWAIT", sceNetInetRecv(server, buf, 64, INET_MSG_DONTWAIT));
		inetPrint("  send 0 bytes", sceNetInetSend(client, buf, 0, 0));
		inetPrint("  Recv after 0 bytes, DONTWAIT", sceNetInetRecv(server, buf, 64, INET_MSG_DONTWAIT));

		printf("\nMSG_WAITALL:\n");
		fill(5, 4);
		sceNetInetSend(client, buf, 5, 0);
		// SO_RCVTIMEO is a 4-byte count of microseconds on the PSP, not a timeval.
		u32 timeout = 300000;
		sceNetInetSetsockopt(server, INET_SOL_SOCKET, INET_SO_RCVTIMEO, &timeout, sizeof(timeout));
		u64 start = sceKernelGetSystemTimeWide();
		int result = sceNetInetRecv(server, buf, 20, INET_MSG_WAITALL);
		u64 took = sceKernelGetSystemTimeWide() - start;
		printf("  WAITALL for 20 with 5 there, 300ms timeout: %d, errno %d, %s\n", result, result < 0 ? sceNetInetGetErrno() : 0,
			took > 250000 ? "waited for the timeout" : "returned early");
		fill(15, 5);
		sceNetInetSend(client, buf, 15, 0);
		recvPrint("  Then recv 64", server, 64, 0);

		printf("\nAddresses:\n");
		InetAddr a;
		int alen = sizeof(a);
		sceNetInetGetpeername(server, &a, &alen);
		InetAddr c;
		int clen = sizeof(c);
		sceNetInetGetsockname(client, &c, &clen);
		printf("  Server's peer is the client's address: %s, alen %d\n", a.port == c.port && a.addr == c.addr ? "yes" : "no", alen);
		printf("  Client's local port is %s\n", inetHtons(c.port) >= 1024 ? "1024 or above" : "below 1024");

		printf("\nHalf-close:\n");
		inetPrint("  shutdown(client, SHUT_WR)", sceNetInetShutdown(client, INET_SHUT_WR));
		recvPrint("  Server recv", server, 64, 0);
		inetPrint("  Client send after SHUT_WR", sceNetInetSend(client, "x", 1, 0));
		inetPrint("  Server send to the half-closed client", sceNetInetSend(server, "y", 1, 0));
		recvPrint("  Client recv", client, 64, 0);
		inetPrint("  shutdown(client, SHUT_RD)", sceNetInetShutdown(client, INET_SHUT_RD));
		sceNetInetSend(server, "z", 1, 0);
		recvPrint("  Client recv after SHUT_RD", client, 64, INET_MSG_DONTWAIT);
		sceNetInetClose(client);
		sceNetInetClose(server);
		sceNetInetClose(listener);
	}

	printf("\nBuffering:\n");
	if (inetTcpPair(port++, &listener, &client, &server)) {
		int nbio = 1;
		sceNetInetSetsockopt(client, INET_SOL_SOCKET, INET_SO_NBIO, &nbio, sizeof(nbio));
		int total = 0;
		int result = 0;
		fill(4096, 6);
		for (int i = 0; i < 200; ++i) {
			result = sceNetInetSend(client, buf, 4096, 0);
			if (result < 0) {
				break;
			}
			total += result;
		}
		printf("  Non-blocking sends until full: %s, then %d, errno %d\n", sizeBucket(total), result, result < 0 ? sceNetInetGetErrno() : 0);
		inetPrint("  select writable", inetWait(client, 1, 100));
		int drained = 0;
		for (;;) {
			int r = sceNetInetRecv(server, buf, sizeof(buf), INET_MSG_DONTWAIT);
			if (r <= 0) {
				if (inetWait(server, 0, 200) <= 0) {
					break;
				}
				continue;
			}
			drained += r;
		}
		printf("  Received: %s, all of it %s\n", sizeBucket(drained), drained == total ? "yes" : "no");
		inetPrint("  select writable after draining", inetWait(client, 1, 100));
		sceNetInetClose(client);
		sceNetInetClose(server);
		sceNetInetClose(listener);
	}

	printf("\nNon-blocking connect:\n");
	listener = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
	InetAddr addr = inetMakeAddr(LOOPBACK, port++);
	sceNetInetBind(listener, &addr, sizeof(addr));
	sceNetInetListen(listener, 2);
	client = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
	int nbio = 1;
	sceNetInetSetsockopt(client, INET_SOL_SOCKET, INET_SO_NBIO, &nbio, sizeof(nbio));
	inetPrint("  connect", sceNetInetConnect(client, &addr, sizeof(addr)));
	inetPrint("  select writable", inetWait(client, 1, 1000));
	int err = -1;
	int len = sizeof(err);
	sceNetInetGetsockopt(client, INET_SOL_SOCKET, INET_SO_ERROR, &err, &len);
	printf("  SO_ERROR: %d\n", err);
	inetPrint("  connect again", sceNetInetConnect(client, &addr, sizeof(addr)));
	sceNetInetSetsockopt(listener, INET_SOL_SOCKET, INET_SO_NBIO, &nbio, sizeof(nbio));
	server = sceNetInetAccept(listener, NULL, NULL);
	printf("  accept: %s\n", server >= 0 ? "ok" : "failed");
	inetPrint("  accept with nothing waiting", sceNetInetAccept(listener, NULL, NULL));
	sceNetInetClose(server);
	sceNetInetClose(client);

	int refused = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
	sceNetInetSetsockopt(refused, INET_SOL_SOCKET, INET_SO_NBIO, &nbio, sizeof(nbio));
	InetAddr nobody = inetMakeAddr(LOOPBACK, port++);
	inetPrint("  connect to a closed port", sceNetInetConnect(refused, &nobody, sizeof(nobody)));
	inetPrint("  select writable", inetWait(refused, 1, 1000));
	err = -1;
	len = sizeof(err);
	sceNetInetGetsockopt(refused, INET_SOL_SOCKET, INET_SO_ERROR, &err, &len);
	printf("  SO_ERROR: %d\n", err);
	sceNetInetClose(refused);

	printf("\nBacklog of 2:\n");
	int clients[5];
	int connected = 0;
	for (int i = 0; i < 5; ++i) {
		clients[i] = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
		sceNetInetSetsockopt(clients[i], INET_SOL_SOCKET, INET_SO_NBIO, &nbio, sizeof(nbio));
		sceNetInetConnect(clients[i], &addr, sizeof(addr));
	}
	sceKernelDelayThread(200000);
	for (int i = 0; i < 5; ++i) {
		InetAddr p;
		int plen = sizeof(p);
		if (sceNetInetGetpeername(clients[i], &p, &plen) == 0) {
			connected++;
		}
	}
	int accepted = 0;
	for (;;) {
		int s = sceNetInetAccept(listener, NULL, NULL);
		if (s < 0) {
			break;
		}
		accepted++;
		sceNetInetClose(s);
	}
	printf("  5 non-blocking connects: %d connected, %d accepted\n", connected, accepted);
	for (int i = 0; i < 5; ++i) {
		sceNetInetClose(clients[i]);
	}
	sceNetInetClose(listener);

	printf("\nAccept's address:\n");
	if (inetTcpPair(port, &listener, &client, &server)) {
		sceNetInetClose(server);
		InetAddr from;
		int fromlen = 4;
		int second = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
		addr = inetMakeAddr(LOOPBACK, port);
		sceNetInetConnect(second, &addr, sizeof(addr));
		memset(&from, 0xCC, sizeof(from));
		int s = sceNetInetAccept(listener, &from, &fromlen);
		printf("  addrlen 4: %s, addrlen now %d, family %d\n", s >= 0 ? "accepted" : "failed", fromlen, from.family);
		sceNetInetClose(s);
		sceNetInetClose(second);
		sceNetInetClose(client);
		sceNetInetClose(listener);
	}
	port++;

	inetShutdown();
	return 0;
}
