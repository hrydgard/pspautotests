// What sceNetAdhocTerm does to the rest of the library: open sockets, the adhocctl state and group,
// matching, and what works again after sceNetAdhocInit.

#include "adhoc.h"

#define PORT 0x1250

static void printState(const char *title) {
	int state = -1;
	int result = sceNetAdhocctlGetState(&state);
	AdhocMac mac;
	int macResult = sceNetGetLocalEtherAddr(&mac);
	printf("%s: GetState %08x, state %d, GetLocalEtherAddr %08x\n", title, result, state, macResult);
}

static void matchingHandler(int id, int event, AdhocMac *mac, int optlen, void *opt) {
}

int main(int argc, char *argv[]) {
	if (!adhocStartup()) {
		return 1;
	}
	printf("sceNetAdhocInit: %08x\n", sceNetAdhocInit());
	if (!adhocctlStartup()) {
		adhocShutdown();
		return 1;
	}
	adhocctlCreateGroup("ADTERM");
	printState("In the group");

	int pdp = sceNetAdhocPdpCreate(&localMac, PORT, 0x2000, 0);
	int ptp = sceNetAdhocPtpListen(&localMac, PORT + 1, 0x2000, 200000, 5, 1, 0);
	printf("Sockets: PDP %d, PTP %d\n", pdp, ptp);
	printf("MatchingInit: %08x\n", sceNetAdhocMatchingInit(0x20000));
	int matching = sceNetAdhocMatchingCreate(1, 2, PORT + 2, 1024, 200000, 500000, 5, 200000, &matchingHandler);
	printf("MatchingCreate: %s\n", matching > 0 ? "ok" : "failed");

	printf("\nsceNetAdhocTerm: %08x\n", sceNetAdhocTerm());
	printState("After AdhocTerm");
	u8 data[4] = { 1, 2, 3, 4 };
	printf("PdpSend on the old socket: %08x\n", sceNetAdhocPdpSend(pdp, &localMac, PORT, data, sizeof(data), 0, 1));
	printf("PdpDelete on the old socket: %08x\n", sceNetAdhocPdpDelete(pdp, 0));
	printf("PtpClose on the old socket: %08x\n", sceNetAdhocPtpClose(ptp, 0));
	printf("PdpCreate before AdhocInit: %08x\n", sceNetAdhocPdpCreate(&localMac, PORT, 0x2000, 0));
	printf("MatchingDelete: %08x\n", sceNetAdhocMatchingDelete(matching));
	printf("MatchingTerm: %08x\n", sceNetAdhocMatchingTerm());

	printf("\nsceNetAdhocInit: %08x\n", sceNetAdhocInit());
	printState("After AdhocInit");
	int pdp2 = sceNetAdhocPdpCreate(&localMac, PORT, 0x2000, 0);
	printf("PdpCreate: %d\n", pdp2);
	if (pdp2 > 0) {
		int len = sizeof(data);
		printf("PdpSend to self: %08x\n", sceNetAdhocPdpSend(pdp2, &localMac, PORT, data, sizeof(data), 0, 1));
		sceKernelDelayThread(100000);
		AdhocMac from;
		u16 fromPort;
		u8 buf[16];
		printf("PdpRecv: %08x, len %d\n", sceNetAdhocPdpRecv(pdp2, &from, &fromPort, buf, &len, 0, 1), len);
		sceNetAdhocPdpDelete(pdp2, 0);
	}

	printf("\nsceNetAdhocctlTerm: %08x\n", sceNetAdhocctlTerm());
	printState("After AdhocctlTerm");
	printf("sceNetAdhocTerm: %08x\n", sceNetAdhocTerm());

	adhocShutdown();
	return 0;
}
