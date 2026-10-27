// Socket alerts on PTP sockets, on one console in a group of its own: the accept, connect, send,
// receive and flush alerts, on calls that would block and on threads already blocked.

#include "adhoc.h"

#define PORT 0x6000

static volatile int threadResult;
static volatile int threadDone;
static int threadSock;

static int acceptThread(SceSize argc, void *argv) {
	threadResult = sceNetAdhocPtpAccept(threadSock, NULL, NULL, 2000000, 0);
	threadDone = 1;
	return 0;
}

static int recvThread(SceSize argc, void *argv) {
	u8 buf[64];
	int len = sizeof(buf);
	threadResult = sceNetAdhocPtpRecv(threadSock, buf, &len, 2000000, 0);
	threadDone = 1;
	return 0;
}

// Starts a thread blocked on sock, sets the alert after 100ms, and reports how the thread woke.
static void testWake(const char *title, int sock, SceKernelThreadEntry entry, int alert) {
	threadSock = sock;
	threadDone = 0;
	threadResult = 0x1234;
	SceUID thread = sceKernelCreateThread("blocked", entry, 0x18, 0x4000, 0, NULL);
	sceKernelStartThread(thread, 0, NULL);
	sceKernelDelayThread(100000);
	int before = threadDone;
	sceNetAdhocSetSocketAlert(sock, alert);
	for (int i = 0; i < 50 && !threadDone; ++i) {
		sceKernelDelayThread(10000);
	}
	printf("%s: done before %d, within 0.5s %d, result %08x\n", title, before, threadDone, threadResult);
	sceKernelWaitThreadEnd(thread, NULL);
	sceKernelDeleteThread(thread);
	sceNetAdhocSetSocketAlert(sock, 0);
}

int main(int argc, char *argv[]) {
	if (!adhocStartup()) {
		return 1;
	}
	printf("sceNetAdhocInit: %08x\n", sceNetAdhocInit());
	if (!adhocctlStartup() || !adhocctlCreateGroup("ALERTS")) {
		adhocShutdown();
		return 1;
	}

	printf("\nListener:\n");
	int listener = sceNetAdhocPtpListen(&localMac, PORT, 4096, 200000, 5, 2, 0);
	sceNetAdhocSetSocketAlert(listener, 0x100);
	int flags = 0;
	sceNetAdhocGetSocketAlert(listener, &flags);
	printf("  Flags: %08x\n", flags);
	printf("  Accept, non-blocking: %08x\n", sceNetAdhocPtpAccept(listener, NULL, NULL, 0, 1));
	printf("  Accept, blocking: %08x\n", sceNetAdhocPtpAccept(listener, NULL, NULL, 1000, 0));
	sceNetAdhocSetSocketAlert(listener, 0x20);
	printf("  Accept with only the receive alert: %08x\n", sceNetAdhocPtpAccept(listener, NULL, NULL, 1000, 0));
	sceNetAdhocSetSocketAlert(listener, 0);
	testWake("  Blocked accept, accept alert", listener, acceptThread, 0x100);
	testWake("  Blocked accept, receive alert", listener, acceptThread, 0x20);

	printf("\nConnection:\n");
	int client = sceNetAdhocPtpOpen(&localMac, PORT + 1, &localMac, PORT, 4096, 200000, 5, 0);
	int server = sceNetAdhocPtpAccept(listener, NULL, NULL, 1000000, 0);
	printf("  Connected: %s\n", server > 0 ? "yes" : "no");
	sceNetAdhocSetSocketAlert(client, 0x80);
	printf("  Connect with the connect alert: %08x\n", sceNetAdhocPtpConnect(client, 1000, 0));
	sceNetAdhocSetSocketAlert(client, 0x10);
	int len = 4;
	printf("  Send with the send alert: %08x\n", sceNetAdhocPtpSend(client, "data", &len, 1000, 0));
	len = 4;
	printf("  Send with the send alert, non-blocking: %08x\n", sceNetAdhocPtpSend(client, "data", &len, 0, 1));
	sceNetAdhocSetSocketAlert(client, 0x200);
	len = 4;
	printf("  Send with only the flush alert: %08x\n", sceNetAdhocPtpSend(client, "data", &len, 1000, 0));
	printf("  Flush with the flush alert: %08x\n", sceNetAdhocPtpFlush(client, 1000, 0));
	sceNetAdhocSetSocketAlert(client, 0);
	printf("  Flush, no alert: %08x\n", sceNetAdhocPtpFlush(client, 1000000, 0));
	u8 buf[64];
	len = sizeof(buf);
	printf("  Server recv: %08x, len %d\n", sceNetAdhocPtpRecv(server, buf, &len, 1000000, 0), len);
	sceNetAdhocSetSocketAlert(server, 0x20);
	len = sizeof(buf);
	printf("  Recv with the receive alert: %08x\n", sceNetAdhocPtpRecv(server, buf, &len, 1000, 0));
	sceNetAdhocSetSocketAlert(server, 0);
	testWake("  Blocked recv, receive alert", server, recvThread, 0x20);
	testWake("  Blocked recv, flush alert", server, recvThread, 0x200);
	len = 4;
	sceNetAdhocPtpSend(client, "late", &len, 0, 1);
	len = sizeof(buf);
	printf("  Recv after clearing: %08x, len %d\n", sceNetAdhocPtpRecv(server, buf, &len, 1000000, 0), len);

	printf("\nPoll alert on PTP:\n");
	sceNetAdhocSetSocketAlert(server, 0x40);
	AdhocPollSd sd = { server, 0x0FFF, 0 };
	int result = sceNetAdhocPollSocket(&sd, 1, 0, 1);
	printf("  Poll: %08x, revents %04x\n", result, sd.revents);
	sceNetAdhocSetSocketAlert(server, 0);

	sceNetAdhocPtpClose(server, 0);
	sceNetAdhocPtpClose(client, 0);
	sceNetAdhocPtpClose(listener, 0);
	printf("\n");
	adhocctlLeaveGroup();
	adhocShutdown();
	return 0;
}
