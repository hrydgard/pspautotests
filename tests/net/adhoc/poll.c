// sceNetAdhocPollSocket and socket alerts on one console, in a group of its own: which events
// PDP and PTP sockets report, and what wakes a thread blocked in a receive.

#include "adhoc.h"

#define PORT 0x5678

#define EV_SEND 0x0001
#define EV_RECV 0x0002
#define EV_CONNECT 0x0004
#define EV_ACCEPT 0x0008
#define EV_FLUSH 0x0010
#define EV_ALL 0x0FFF

static int pdpA, pdpB;

static void testPoll(const char *title, AdhocPollSd *sds, int count, u32 timeout, int flag) {
	for (int i = 0; i < count; ++i) {
		sds[i].revents = 0xCCCC;
	}
	int result = sceNetAdhocPollSocket(sds, count, timeout, flag);
	printf("%s: %08x", title, result);
	for (int i = 0; i < count; ++i) {
		printf(", revents %04x", sds[i].revents);
	}
	printf("\n");
}

static void pollOne(const char *title, int id, int events) {
	AdhocPollSd sd = { id, events, 0 };
	testPoll(title, &sd, 1, 0, 1);
}

static void testSetAlert(const char *title, int id, int flags) {
	int result = sceNetAdhocSetSocketAlert(id, flags);
	int got = 0xCCCC;
	int getResult = sceNetAdhocGetSocketAlert(id, &got);
	printf("%s: %08x, get %08x, flags %08x\n", title, result, getResult, got);
}

static volatile int threadResult;
static volatile int threadDone;

static int recvThread(SceSize argc, void *argv) {
	u8 buf[64];
	int len = sizeof(buf);
	AdhocMac mac;
	u16 port;
	threadResult = sceNetAdhocPdpRecv(*(int *)argv, &mac, &port, buf, &len, 2000000, 0);
	threadDone = 1;
	return 0;
}

// Starts a thread blocked in PdpRecv on id, runs action after 100ms, and reports how the thread woke.
static void testWake(const char *title, int id, void (*action)(int id)) {
	threadDone = 0;
	threadResult = 0x1234;
	SceUID thread = sceKernelCreateThread("recv", &recvThread, 0x18, 0x4000, 0, NULL);
	sceKernelStartThread(thread, sizeof(id), &id);
	sceKernelDelayThread(100000);
	int before = threadDone;
	action(id);
	for (int i = 0; i < 100 && !threadDone; ++i) {
		sceKernelDelayThread(10000);
	}
	printf("%s: done before %d, after %d, result %08x\n", title, before, threadDone, threadResult);
	sceKernelWaitThreadEnd(thread, NULL);
	sceKernelDeleteThread(thread);
}

static void alertRecv(int id) {
	printf("  SetSocketAlert: %08x\n", sceNetAdhocSetSocketAlert(id, 0x20));
}

static void alertSend(int id) {
	printf("  SetSocketAlert: %08x\n", sceNetAdhocSetSocketAlert(id, 0x10));
}

static void deleteSocket(int id) {
	printf("  PdpDelete: %08x\n", sceNetAdhocPdpDelete(id, 0));
}

int main(int argc, char *argv[]) {
	if (!adhocStartup()) {
		return 1;
	}
	printf("sceNetAdhocInit: %08x\n", sceNetAdhocInit());
	if (!adhocctlStartup() || !adhocctlCreateGroup("POLLTEST")) {
		adhocShutdown();
		return 1;
	}

	pdpA = sceNetAdhocPdpCreate(&localMac, PORT, 0x2000, 0);
	pdpB = sceNetAdhocPdpCreate(&localMac, PORT + 1, 0x2000, 0);

	printf("\nAlerts:\n");
	int flags = 0xCCCC;
	printf("  Get initial: %08x, flags %08x\n", sceNetAdhocGetSocketAlert(pdpA, &flags), flags);
	testSetAlert("  Recv", pdpA, 0x20);
	testSetAlert("  All", pdpA, 0x3F0);
	testSetAlert("  Bit 0", pdpA, 0x1);
	testSetAlert("  Bit 0x400", pdpA, 0x400);
	testSetAlert("  All bits", pdpA, -1);
	testSetAlert("  Zero", pdpA, 0);
	testSetAlert("  Invalid id", 0, 0x20);
	testSetAlert("  Unused id", 1000, 0x20);
	printf("  Get NULL: %08x\n", sceNetAdhocGetSocketAlert(pdpA, NULL));

	printf("\nAlerted calls:\n");
	u8 buf[64];
	int len = sizeof(buf);
	AdhocMac mac;
	u16 port;
	sceNetAdhocSetSocketAlert(pdpB, 0x20);
	printf("  Recv, non-blocking: %08x\n", sceNetAdhocPdpRecv(pdpB, &mac, &port, buf, &len, 0, 1));
	len = sizeof(buf);
	printf("  Recv, blocking: %08x\n", sceNetAdhocPdpRecv(pdpB, &mac, &port, buf, &len, 1000, 0));
	len = sizeof(buf);
	printf("  Recv again: %08x\n", sceNetAdhocPdpRecv(pdpB, &mac, &port, buf, &len, 1000, 0));
	flags = 0xCCCC;
	sceNetAdhocGetSocketAlert(pdpB, &flags);
	printf("  Flags afterwards: %08x\n", flags);
	printf("  Send, blocking: %08x\n", sceNetAdhocPdpSend(pdpB, &localMac, PORT, "x", 1, 1000, 0));
	pollOne("  Poll RECV", pdpB, EV_RECV);
	sceNetAdhocSetSocketAlert(pdpB, 0x40);
	pollOne("  Poll with poll alert, non-blocking", pdpB, EV_RECV);
	AdhocPollSd alertSd = { pdpB, EV_RECV, 0 };
	testPoll("  Poll with poll alert, blocking", &alertSd, 1, 1000, 0);
	sceNetAdhocSetSocketAlert(pdpB, 0x10);
	printf("  Send with send alert, blocking: %08x\n", sceNetAdhocPdpSend(pdpB, &localMac, PORT, "x", 1, 1000, 0));
	printf("  Send with send alert, non-blocking: %08x\n", sceNetAdhocPdpSend(pdpB, &localMac, PORT, "x", 1, 0, 1));
	sceNetAdhocSetSocketAlert(pdpB, 0);
	len = sizeof(buf);
	while (sceNetAdhocPdpRecv(pdpA, &mac, &port, buf, &len, 0, 1) == 0) {
		len = sizeof(buf);
	}

	printf("\nPoll, PDP:\n");
	pollOne("  Nothing waiting, RECV", pdpB, EV_RECV);
	pollOne("  Nothing waiting, SEND", pdpB, EV_SEND);
	pollOne("  Nothing waiting, all", pdpB, EV_ALL);
	pollOne("  Nothing waiting, none", pdpB, 0);
	sceNetAdhocPdpSend(pdpA, &localMac, PORT + 1, "data", 4, 0, 1);
	sceKernelDelayThread(10000);
	pollOne("  Data waiting, RECV", pdpB, EV_RECV);
	pollOne("  Data waiting, all", pdpB, EV_ALL);
	AdhocPollSd two[2] = { { pdpA, EV_RECV, 0 }, { pdpB, EV_RECV, 0 } };
	testPoll("  Two sockets", two, 2, 0, 1);
	len = sizeof(buf);
	sceNetAdhocPdpRecv(pdpB, &mac, &port, buf, &len, 0, 1);
	AdhocPollSd one = { pdpB, EV_RECV, 0 };
	testPoll("  Blocking, 1ms", &one, 1, 1000, 0);
	testPoll("  Count 0", &one, 0, 0, 1);
	testPoll("  Count -1", &one, -1, 0, 1);
	printf("  NULL: %08x\n", sceNetAdhocPollSocket(NULL, 1, 0, 1));
	testPoll("  Flag 2", &one, 1, 0, 2);
	AdhocPollSd bad[2] = { { pdpB, EV_RECV, 0 }, { 1000, EV_RECV, 0 } };
	testPoll("  With an invalid id", bad, 2, 0, 1);

	printf("\nWaking a blocked receive:\n");
	testWake("  Recv alert", pdpB, alertRecv);
	sceNetAdhocSetSocketAlert(pdpB, 0);
	testWake("  Send alert", pdpB, alertSend);
	sceNetAdhocSetSocketAlert(pdpB, 0);
	testWake("  Delete", pdpB, deleteSocket);
	pollOne("  Poll deleted socket", pdpB, EV_RECV);

	printf("\nPoll, PTP:\n");
	int listener = sceNetAdhocPtpListen(&localMac, PORT + 2, 4096, 200000, 5, 1, 0);
	pollOne("  Listener, nothing waiting", listener, EV_ALL);
	int client = sceNetAdhocPtpOpen(&localMac, PORT + 3, &localMac, PORT + 2, 4096, 200000, 5, 0);
	sceKernelDelayThread(10000);
	pollOne("  Listener, connection waiting", listener, EV_ALL);
	pollOne("  Client", client, EV_ALL);
	int accepted = sceNetAdhocPtpAccept(listener, NULL, NULL, 1000000, 0);
	pollOne("  Accepted", accepted, EV_ALL);
	len = 4;
	sceNetAdhocPtpSend(client, "data", &len, 0, 1);
	sceKernelDelayThread(10000);
	pollOne("  Accepted, data waiting", accepted, EV_ALL);
	pollOne("  Accepted, data waiting, SEND only", accepted, EV_SEND);
	sceNetAdhocPtpClose(client, 0);
	sceKernelDelayThread(100000);
	pollOne("  Accepted, peer closed", accepted, EV_ALL);
	pollOne("  Accepted, peer closed, none", accepted, 0);
	int nobody = sceNetAdhocPtpOpen(&localMac, PORT + 4, &localMac, PORT + 5, 4096, 200000, 2, 0);
	sceKernelDelayThread(100000);
	pollOne("  Refused connect", nobody, EV_ALL);
	sceNetAdhocPtpClose(nobody, 0);
	sceNetAdhocPtpClose(accepted, 0);
	sceNetAdhocPtpClose(listener, 0);

	sceNetAdhocPdpDelete(pdpA, 0);
	printf("\n");
	adhocctlLeaveGroup();
	adhocShutdown();
	return 0;
}
