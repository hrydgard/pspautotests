// sceNetInetSendmsg and sceNetInetRecvmsg over loopback: gathering and scattering, the address and
// its length, truncation and msg_flags, and what becomes of control data. Also sceNetInetSendto
// without an address.

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

#define PORT_A 24400
#define PORT_B 24401
#define PORT_TCP 24402

static int udpSocket(u16 port) {
	int s = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	InetAddr addr = inetMakeAddr(LOOPBACK, port);
	sceNetInetBind(s, &addr, sizeof(addr));
	return s;
}

// Receives whatever is waiting on s, without blocking, and prints it.
static void drain(const char *title, int s) {
	char buf[64];
	memset(buf, 0, sizeof(buf));
	int ready = inetWait(s, 0, 200);
	int result = ready > 0 ? sceNetInetRecv(s, buf, sizeof(buf) - 1, INET_MSG_DONTWAIT) : -1;
	if (result < 0) {
		printf("%s: nothing, ready %d\n", title, ready);
	} else {
		printf("%s: %d '%s'\n", title, result, buf);
	}
}

static void printMsg(const char *title, int result, const InetMsghdr *msg, const InetAddr *from) {
	if (result < 0) {
		printf("%s: %d, errno %d", title, result, sceNetInetGetErrno());
	} else {
		printf("%s: %d", title, result);
	}
	printf(", namelen %d, controllen %d, flags %08x", msg->namelen, msg->controllen, msg->flags);
	if (from) {
		printf(", from len %02x family %02x port %s", from->len, from->family, inetHtons(from->port) == PORT_A ? "A" : (from->port == 0xCCCC ? "untouched" : "other"));
	}
	printf("\n");
}

int main(int argc, char *argv[]) {
	if (!inetStartup()) {
		return 1;
	}

	int a = udpSocket(PORT_A);
	int b = udpSocket(PORT_B);
	InetAddr addrB = inetMakeAddr(LOOPBACK, PORT_B);
	char p1[] = "hel", p2[] = "lo, ", p3[] = "world";

	printf("Sendmsg, UDP:\n");
	InetIovec gather[3] = { { p1, 3 }, { p2, 4 }, { p3, 5 } };
	InetMsghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.name = &addrB;
	msg.namelen = sizeof(addrB);
	msg.iov = gather;
	msg.iovlen = 3;
	inetPrint("  Three buffers", sceNetInetSendmsg(a, &msg, 0));
	drain("  B receives", b);

	msg.iovlen = 0;
	inetPrint("  No buffers", sceNetInetSendmsg(a, &msg, 0));
	drain("  B receives", b);
	msg.iovlen = 1;

	msg.namelen = 8;
	inetPrint("  namelen 8", sceNetInetSendmsg(a, &msg, 0));
	drain("  B receives", b);
	msg.namelen = sizeof(addrB);

	msg.name = NULL;
	msg.namelen = 0;
	inetPrint("  No address, not connected", sceNetInetSendmsg(a, &msg, 0));
	msg.name = &addrB;
	msg.namelen = sizeof(addrB);

	// A cmsghdr of { len, level, type } and four bytes of data.
	int control[4] = { 16, INET_SOL_SOCKET, 1, 0x12345678 };
	msg.control = control;
	msg.controllen = sizeof(control);
	inetPrint("  With control data", sceNetInetSendmsg(a, &msg, 0));
	drain("  B receives", b);
	msg.controllen = 12;
	control[0] = 12;
	inetPrint("  With an empty control message", sceNetInetSendmsg(a, &msg, 0));
	drain("  B receives", b);
	msg.control = NULL;
	msg.controllen = 0;

	msg.flags = 0x7FFF;
	inetPrint("  msg_flags set", sceNetInetSendmsg(a, &msg, 0));
	drain("  B receives", b);
	msg.flags = 0;

	inetPrint("  Bad socket", sceNetInetSendmsg(200, &msg, 0));

	printf("\nRecvmsg, UDP:\n");
	char r1[4], r2[4], r3[16];
	InetIovec scatter[3] = { { r1, 4 }, { r2, 4 }, { r3, 16 } };
	InetAddr from;
	int ctrl[8];

	sceNetInetSendto(a, "0123456789", 10, 0, &addrB, sizeof(addrB));
	inetWait(b, 0, 200);
	memset(r1, '.', 4); memset(r2, '.', 4); memset(r3, 0, 16);
	memset(&from, 0xCC, sizeof(from));
	memset(&msg, 0, sizeof(msg));
	msg.name = &from;
	msg.namelen = sizeof(from);
	msg.iov = scatter;
	msg.iovlen = 3;
	msg.control = ctrl;
	msg.controllen = sizeof(ctrl);
	msg.flags = 0x5555;
	int result = sceNetInetRecvmsg(b, &msg, 0);
	printMsg("  Three buffers", result, &msg, &from);
	printf("    '%.4s' '%.4s' '%s'\n", r1, r2, r3);

	sceNetInetSendto(a, "abcdefghijklmnopqrst", 20, 0, &addrB, sizeof(addrB));
	inetWait(b, 0, 200);
	memset(r1, '.', 4); memset(r2, '.', 4);
	msg.iovlen = 2;
	msg.namelen = sizeof(from);
	msg.controllen = sizeof(ctrl);
	msg.flags = 0;
	result = sceNetInetRecvmsg(b, &msg, 0);
	printMsg("  20 bytes into 8", result, &msg, &from);
	printf("    '%.4s' '%.4s'\n", r1, r2);
	drain("  Anything left", b);

	sceNetInetSendto(a, "xyz", 3, 0, &addrB, sizeof(addrB));
	inetWait(b, 0, 200);
	memset(&from, 0xCC, sizeof(from));
	msg.iovlen = 3;
	msg.namelen = 4;
	msg.controllen = 0;
	msg.control = NULL;
	msg.flags = 0;
	result = sceNetInetRecvmsg(b, &msg, 0);
	printMsg("  namelen 4", result, &msg, &from);

	sceNetInetSendto(a, "xyz", 3, 0, &addrB, sizeof(addrB));
	inetWait(b, 0, 200);
	msg.name = NULL;
	msg.namelen = sizeof(from);
	result = sceNetInetRecvmsg(b, &msg, 0);
	printMsg("  No address buffer", result, &msg, NULL);

	msg.name = &from;
	msg.namelen = sizeof(from);
	result = sceNetInetRecvmsg(b, &msg, INET_MSG_DONTWAIT);
	printMsg("  Nothing waiting, MSG_DONTWAIT", result, &msg, NULL);
	result = sceNetInetRecvmsg(200, &msg, 0);
	printMsg("  Bad socket", result, &msg, NULL);

	printf("\nRecvmsg, TCP:\n");
	int listener, client, server;
	if (inetTcpPair(PORT_TCP, &listener, &client, &server)) {
		sceNetInetSend(client, "abcdef", 6, 0);
		inetWait(server, 0, 200);
		memset(r3, 0, sizeof(r3));
		memset(&from, 0xCC, sizeof(from));
		InetIovec one = { r3, 15 };
		memset(&msg, 0, sizeof(msg));
		msg.name = &from;
		msg.namelen = sizeof(from);
		msg.iov = &one;
		msg.iovlen = 1;
		msg.flags = 0x5555;
		result = sceNetInetRecvmsg(server, &msg, 0);
		printMsg("  Recvmsg", result, &msg, NULL);
		printf("    '%s', address %s\n", r3, from.port == 0xCCCC ? "untouched" : "written");

		InetIovec two[2] = { { p1, 3 }, { p2, 4 } };
		int tcpControl[4] = { 16, INET_SOL_SOCKET, 1, 0x12345678 };
		memset(&msg, 0, sizeof(msg));
		msg.iov = two;
		msg.iovlen = 2;
		inetPrint("  Sendmsg, two buffers", sceNetInetSendmsg(client, &msg, 0));
		drain("  Server receives", server);
		msg.control = tcpControl;
		msg.controllen = sizeof(tcpControl);
		inetPrint("  Sendmsg with control data", sceNetInetSendmsg(client, &msg, 0));
		drain("  Server receives", server);
		sceNetInetClose(client);
		sceNetInetClose(server);
	} else {
		printf("  TCP pair failed\n");
	}
	sceNetInetClose(listener);

	printf("\nSendto without an address:\n");
	inetPrint("  Not connected", sceNetInetSendto(a, "n", 1, 0, NULL, 0));
	inetPrint("  Address, length 0", sceNetInetSendto(a, "z", 1, 0, &addrB, 0));
	drain("  B receives", b);
	sceNetInetConnect(a, &addrB, sizeof(addrB));
	inetPrint("  Connected", sceNetInetSendto(a, "c", 1, 0, NULL, 0));
	drain("  B receives", b);

	sceNetInetClose(a);
	sceNetInetClose(b);
	inetShutdown();
	return 0;
}
