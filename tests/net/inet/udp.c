// UDP over loopback: what recvfrom does with a buffer too small for the datagram, MSG_PEEK,
// zero-length and large datagrams, connected UDP sockets, and the source address it reports.

#include "inet.h"

#define PORT_A 24300
#define PORT_B 24301

static u8 buf[70000];

static void fill(int len, u8 seed) {
	for (int i = 0; i < len; ++i) {
		buf[i] = (u8)(i + seed);
	}
}

static int recvOne(const char *title, int s, int size, int flags) {
	memset(buf, 0, size);
	InetAddr from;
	memset(&from, 0xCC, sizeof(from));
	int fromlen = sizeof(from);
	int ready = inetWait(s, 0, 1000);
	int result = ready > 0 ? sceNetInetRecvfrom(s, buf, size, flags, &from, &fromlen) : -1;
	if (result < 0) {
		printf("%s: %d, ready %d, errno %d\n", title, result, ready, ready > 0 ? sceNetInetGetErrno() : 0);
	} else {
		// A port the stack picked differs from run to run, so only the fixed ones are printed.
		u16 p = inetHtons(from.port);
		char portStr[16];
		snprintf(portStr, sizeof(portStr), "%d", p);
		printf("%s: %d, from %s port %s, fromlen %d, first byte %02x\n", title, result, inetHtonl(from.addr) == LOOPBACK ? "127.0.0.1" : "other",
			p == PORT_A || p == PORT_B ? portStr : "picked", fromlen, buf[0]);
	}
	return result;
}

int main(int argc, char *argv[]) {
	if (!inetStartup()) {
		return 1;
	}
	int a = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	int b = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	InetAddr addrA = inetMakeAddr(LOOPBACK, PORT_A);
	InetAddr addrB = inetMakeAddr(LOOPBACK, PORT_B);
	sceNetInetBind(a, &addrA, sizeof(addrA));
	sceNetInetBind(b, &addrB, sizeof(addrB));

	printf("Truncation:\n");
	fill(100, 1);
	inetPrint("  sendto 100", sceNetInetSendto(a, buf, 100, 0, &addrB, sizeof(addrB)));
	recvOne("  recvfrom into 10", b, 10, 0);
	printf("  Anything left: %d\n", inetWait(b, 0, 200));

	printf("\nMSG_PEEK:\n");
	fill(50, 2);
	sceNetInetSendto(a, buf, 50, 0, &addrB, sizeof(addrB));
	recvOne("  Peek", b, 64, INET_MSG_PEEK);
	recvOne("  Peek into 10", b, 10, INET_MSG_PEEK);
	recvOne("  Receive", b, 64, 0);
	printf("  Anything left: %d\n", inetWait(b, 0, 200));

	printf("\nOrder and boundaries:\n");
	for (int i = 0; i < 3; ++i) {
		fill(10 + i, (u8)(0x10 * i));
		sceNetInetSendto(a, buf, 10 + i, 0, &addrB, sizeof(addrB));
	}
	recvOne("  First", b, 64, 0);
	recvOne("  Second", b, 64, 0);
	recvOne("  Third", b, 64, 0);

	printf("\nSizes:\n");
	inetPrint("  sendto 0", sceNetInetSendto(a, buf, 0, 0, &addrB, sizeof(addrB)));
	recvOne("  recvfrom", b, 64, 0);
	inetPrint("  sendto 8192", sceNetInetSendto(a, buf, 8192, 0, &addrB, sizeof(addrB)));
	recvOne("  recvfrom", b, sizeof(buf), 0);
	inetPrint("  sendto 9216", sceNetInetSendto(a, buf, 9216, 0, &addrB, sizeof(addrB)));
	recvOne("  recvfrom", b, sizeof(buf), 0);
	inetPrint("  sendto 9217", sceNetInetSendto(a, buf, 9217, 0, &addrB, sizeof(addrB)));
	recvOne("  recvfrom", b, sizeof(buf), 0);
	inetPrint("  sendto 65507", sceNetInetSendto(a, buf, 65507, 0, &addrB, sizeof(addrB)));
	recvOne("  recvfrom", b, sizeof(buf), 0);
	inetPrint("  sendto 65508", sceNetInetSendto(a, buf, 65508, 0, &addrB, sizeof(addrB)));
	int big = 1 << 15;
	inetPrint("  setsockopt SO_SNDBUF 32K", sceNetInetSetsockopt(a, INET_SOL_SOCKET, INET_SO_SNDBUF, &big, sizeof(big)));
	inetPrint("  setsockopt SO_RCVBUF 32K", sceNetInetSetsockopt(b, INET_SOL_SOCKET, INET_SO_RCVBUF, &big, sizeof(big)));
	inetPrint("  sendto 20000 after", sceNetInetSendto(a, buf, 20000, 0, &addrB, sizeof(addrB)));
	recvOne("  recvfrom", b, sizeof(buf), 0);

	printf("\nfromlen:\n");
	fill(4, 3);
	sceNetInetSendto(a, buf, 4, 0, &addrB, sizeof(addrB));
	InetAddr from;
	memset(&from, 0xCC, sizeof(from));
	int fromlen = 4;
	inetWait(b, 0, 1000);
	int result = sceNetInetRecvfrom(b, buf, 64, 0, &from, &fromlen);
	printf("  fromlen 4: %d, fromlen now %d, port bytes %s\n", result, fromlen, from.port == 0xCCCC ? "untouched" : "written");
	sceNetInetSendto(a, buf, 4, 0, &addrB, sizeof(addrB));
	inetWait(b, 0, 1000);
	inetPrint("  NULL from", sceNetInetRecvfrom(b, buf, 64, 0, NULL, NULL));

	printf("\nConnected UDP:\n");
	int c = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	inetPrint("  connect to B", sceNetInetConnect(c, &addrB, sizeof(addrB)));
	InetAddr name;
	int namelen = sizeof(name);
	sceNetInetGetsockname(c, &name, &namelen);
	printf("  Bound to %s, port %s\n", inetHtonl(name.addr) == LOOPBACK ? "127.0.0.1" : (name.addr == 0 ? "0.0.0.0" : "other"), name.port ? "picked" : "0");
	int peerlen = sizeof(name);
	inetPrint("  getpeername", sceNetInetGetpeername(c, &name, &peerlen));
	printf("    port %d\n", inetHtons(name.port));
	fill(5, 4);
	inetPrint("  send", sceNetInetSend(c, buf, 5, 0));
	recvOne("  B receives", b, 64, 0);
	inetPrint("  sendto another address", sceNetInetSendto(c, buf, 5, 0, &addrA, sizeof(addrA)));
	recvOne("  A receives", a, 64, INET_MSG_DONTWAIT);
	// Something from A, which c isn't connected to, shouldn't reach it.
	InetAddr cAddr = name;
	namelen = sizeof(cAddr);
	sceNetInetGetsockname(c, &cAddr, &namelen);
	sceNetInetSendto(a, buf, 5, 0, &cAddr, sizeof(cAddr));
	recvOne("  c receives from A", c, 64, INET_MSG_DONTWAIT);
	sceNetInetSendto(b, buf, 5, 0, &cAddr, sizeof(cAddr));
	recvOne("  c receives from B", c, 64, 0);

	printf("\nSending to a closed port:\n");
	InetAddr nobody = inetMakeAddr(LOOPBACK, PORT_B + 10);
	int d = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	inetPrint("  connect", sceNetInetConnect(d, &nobody, sizeof(nobody)));
	inetPrint("  send", sceNetInetSend(d, buf, 5, 0));
	sceKernelDelayThread(50000);
	inetPrint("  recv afterwards", sceNetInetRecv(d, buf, 64, INET_MSG_DONTWAIT));
	inetPrint("  send again", sceNetInetSend(d, buf, 5, 0));
	inetPrint("  sendto unconnected", sceNetInetSendto(a, buf, 5, 0, &nobody, sizeof(nobody)));
	sceKernelDelayThread(50000);
	inetPrint("  recvfrom afterwards", sceNetInetRecvfrom(a, buf, 64, INET_MSG_DONTWAIT, NULL, NULL));

	printf("\nBroadcast on loopback:\n");
	InetAddr bcast = inetMakeAddr(0xFFFFFFFF, PORT_B);
	inetPrint("  sendto 255.255.255.255 without SO_BROADCAST", sceNetInetSendto(a, buf, 5, 0, &bcast, sizeof(bcast)));
	int one = 1;
	sceNetInetSetsockopt(a, INET_SOL_SOCKET, INET_SO_BROADCAST, &one, sizeof(one));
	inetPrint("  sendto 255.255.255.255 with SO_BROADCAST", sceNetInetSendto(a, buf, 5, 0, &bcast, sizeof(bcast)));
	recvOne("  B receives", b, 64, 0);

	sceNetInetClose(a);
	sceNetInetClose(b);
	sceNetInetClose(c);
	sceNetInetClose(d);
	inetShutdown();
	return 0;
}
