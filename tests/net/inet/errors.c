// What sceNetInet returns, and what errno it sets, for the common ways to misuse a socket: bad
// arguments to socket(), invalid and closed descriptors, an address in use, a refused connection,
// calls on sockets in the wrong state, and a peer that closes or resets the connection. Everything
// goes through the loopback interface, each case on its own port.

#include "inet.h"

static int port = 24100;

static void testSocket(const char *title, int domain, int type, int protocol) {
	int s = sceNetInetSocket(domain, type, protocol);
	if (s >= 0) {
		printf("%s: a socket\n", title);
		sceNetInetClose(s);
	} else {
		inetPrint(title, s);
	}
}

int main(int argc, char *argv[]) {
	if (!inetStartup()) {
		return 1;
	}
	char buf[64];
	InetAddr addr;
	int addrlen;

	printf("socket:\n");
	testSocket("  TCP", INET_AF_INET, INET_SOCK_STREAM, 0);
	testSocket("  UDP", INET_AF_INET, INET_SOCK_DGRAM, 0);
	testSocket("  TCP, IPPROTO_TCP", INET_AF_INET, INET_SOCK_STREAM, INET_IPPROTO_TCP);
	testSocket("  UDP, IPPROTO_UDP", INET_AF_INET, INET_SOCK_DGRAM, INET_IPPROTO_UDP);
	testSocket("  TCP, IPPROTO_UDP", INET_AF_INET, INET_SOCK_STREAM, INET_IPPROTO_UDP);
	testSocket("  Raw", INET_AF_INET, INET_SOCK_RAW, 0);
	testSocket("  Domain 0", 0, INET_SOCK_STREAM, 0);
	testSocket("  Domain 23 (INET6)", 23, INET_SOCK_STREAM, 0);
	testSocket("  Type 0", INET_AF_INET, 0, 0);
	testSocket("  Type 99", INET_AF_INET, 99, 0);
	testSocket("  Protocol 99", INET_AF_INET, INET_SOCK_STREAM, 99);

	printf("\nInvalid descriptors:\n");
	inetPrint("  close -1", sceNetInetClose(-1));
	inetPrint("  close 200", sceNetInetClose(200));
	inetPrint("  send on 200", sceNetInetSend(200, "x", 1, 0));
	inetPrint("  recv on 200", sceNetInetRecv(200, buf, sizeof(buf), 0));
	int s = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	inetPrint("  close", sceNetInetClose(s));
	inetPrint("  close again", sceNetInetClose(s));
	inetPrint("  sendto on the closed socket", sceNetInetSendto(s, "x", 1, 0, &addr, sizeof(addr)));

	printf("\nbind:\n");
	int a = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	int b = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	addr = inetMakeAddr(LOOPBACK, port);
	inetPrint("  UDP", sceNetInetBind(a, &addr, sizeof(addr)));
	inetPrint("  Same address again, other socket", sceNetInetBind(b, &addr, sizeof(addr)));
	InetAddr other = inetMakeAddr(LOOPBACK, port + 1);
	inetPrint("  The same socket again, other port", sceNetInetBind(a, &other, sizeof(other)));
	inetPrint("  addrlen 4", sceNetInetBind(b, &other, 4));
	inetPrint("  addrlen 0", sceNetInetBind(b, &other, 0));
	// bind with a NULL address hangs the console, so it isn't tried.
	InetAddr wrongFamily = other;
	wrongFamily.family = 1;
	inetPrint("  Wrong family", sceNetInetBind(b, &wrongFamily, sizeof(addr)));
	InetAddr foreign = inetMakeAddr(0x0A0B0C0D, port + 2);
	inetPrint("  An address the console doesn't have", sceNetInetBind(b, &foreign, sizeof(foreign)));
	sceNetInetClose(a);
	sceNetInetClose(b);
	port += 3;

	printf("\nTCP in the wrong state:\n");
	s = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
	inetPrint("  send, not connected", sceNetInetSend(s, "x", 1, 0));
	inetPrint("  recv, not connected", sceNetInetRecv(s, buf, sizeof(buf), INET_MSG_DONTWAIT));
	addrlen = sizeof(addr);
	inetPrint("  getpeername, not connected", sceNetInetGetpeername(s, &addr, &addrlen));
	inetPrint("  accept, not listening", sceNetInetAccept(s, NULL, NULL));
	inetPrint("  shutdown, not connected", sceNetInetShutdown(s, INET_SHUT_RDWR));
	inetPrint("  listen, not bound", sceNetInetListen(s, 1));
	inetPrint("  listen again", sceNetInetListen(s, 1));
	addr = inetMakeAddr(LOOPBACK, port++);
	inetPrint("  connect, while listening", sceNetInetConnect(s, &addr, sizeof(addr)));
	sceNetInetClose(s);

	printf("\nUDP in the wrong state:\n");
	s = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	inetPrint("  listen", sceNetInetListen(s, 1));
	inetPrint("  accept", sceNetInetAccept(s, NULL, NULL));
	inetPrint("  send, not connected", sceNetInetSend(s, "x", 1, 0));
	addrlen = sizeof(addr);
	inetPrint("  getpeername, not connected", sceNetInetGetpeername(s, &addr, &addrlen));
	inetPrint("  shutdown", sceNetInetShutdown(s, INET_SHUT_RDWR));
	inetPrint("  shutdown, how 3", sceNetInetShutdown(s, 3));
	sceNetInetClose(s);

	printf("\nConnecting to a port nobody listens on:\n");
	s = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
	addr = inetMakeAddr(LOOPBACK, port++);
	inetPrint("  connect", sceNetInetConnect(s, &addr, sizeof(addr)));
	inetPrint("  connect again", sceNetInetConnect(s, &addr, sizeof(addr)));
	int err = -1;
	int len = sizeof(err);
	inetPrint("  getsockopt SO_ERROR", sceNetInetGetsockopt(s, INET_SOL_SOCKET, INET_SO_ERROR, &err, &len));
	printf("    SO_ERROR %d\n", err);
	sceNetInetClose(s);

	printf("\nThe peer closes:\n");
	int listener, client, server;
	if (inetTcpPair(port++, &listener, &client, &server)) {
		sceNetInetClose(server);
		sceKernelDelayThread(50000);
		inetPrint("  recv", sceNetInetRecv(client, buf, sizeof(buf), 0));
		inetPrint("  recv again", sceNetInetRecv(client, buf, sizeof(buf), 0));
		inetPrint("  send", sceNetInetSend(client, "x", 1, 0));
		sceKernelDelayThread(50000);
		inetPrint("  send again", sceNetInetSend(client, "x", 1, 0));
		addrlen = sizeof(addr);
		inetPrint("  getpeername", sceNetInetGetpeername(client, &addr, &addrlen));
		sceNetInetClose(client);
		sceNetInetClose(listener);
	}

	printf("\nThe peer resets:\n");
	if (inetTcpPair(port++, &listener, &client, &server)) {
		inetPrint("  CloseWithRST", sceNetInetCloseWithRST(server));
		sceKernelDelayThread(50000);
		inetPrint("  recv", sceNetInetRecv(client, buf, sizeof(buf), 0));
		inetPrint("  recv again", sceNetInetRecv(client, buf, sizeof(buf), 0));
		inetPrint("  send", sceNetInetSend(client, "x", 1, 0));
		sceNetInetClose(client);
		sceNetInetClose(listener);
	}

	printf("\nDescriptor numbers:\n");
	int fds[4];
	for (int i = 0; i < 4; ++i) {
		fds[i] = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	}
	printf("  Consecutive: %s\n", fds[1] == fds[0] + 1 && fds[2] == fds[1] + 1 && fds[3] == fds[2] + 1 ? "yes" : "no");
	sceNetInetClose(fds[1]);
	s = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	printf("  After closing the second, a new one gets %s\n", s == fds[1] ? "its number" : (s > fds[3] ? "a higher number" : "another number"));
	sceNetInetClose(s);
	for (int i = 0; i < 4; ++i) {
		if (i != 1) {
			sceNetInetClose(fds[i]);
		}
	}
	int count = 0;
	int last = 0;
	int all[300];
	for (count = 0; count < 300; ++count) {
		all[count] = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
		if (all[count] < 0) {
			last = all[count];
			break;
		}
	}
	printf("  Sockets until it fails: %d, then %d, errno %d\n", count, last, count < 300 ? sceNetInetGetErrno() : 0);
	for (int i = 0; i < count; ++i) {
		sceNetInetClose(all[i]);
	}

	printf("\nerrno:\n");
	sceNetInetClose(200);
	int first = sceNetInetGetErrno();
	s = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	printf("  After a failure then a success: %s\n", sceNetInetGetErrno() == first ? "unchanged" : "changed");
	sceNetInetClose(s);
	printf("  GetPspError: %08x\n", sceNetInetGetPspError());

	inetShutdown();
	return 0;
}
