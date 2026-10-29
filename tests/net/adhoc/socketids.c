// Which socket IDs PdpCreate and PtpListen hand out, and whether sceNetAdhocTerm and sceNetAdhocInit
// (also with sceNetTerm and sceNetInit in between, like a game that leaves its lobby for a match)
// start them over. Prints the raw IDs.

#include "adhoc.h"

#define PORT 0x1240

static void createSome(const char *title) {
	int a = sceNetAdhocPdpCreate(&localMac, PORT, 0x2000, 0);
	int b = sceNetAdhocPdpCreate(&localMac, PORT + 1, 0x2000, 0);
	int c = sceNetAdhocPtpListen(&localMac, PORT + 2, 0x2000, 200000, 5, 1, 0);
	printf("%s: PDP %d, PDP %d, PTP listen %d\n", title, a, b, c);
	if (a > 0) {
		sceNetAdhocPdpDelete(a, 0);
	}
	if (b > 0) {
		sceNetAdhocPdpDelete(b, 0);
	}
	if (c > 0) {
		sceNetAdhocPtpClose(c, 0);
	}
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

	adhocctlCreateGroup("SOCKIDS");
	createSome("First");
	createSome("Again, after deleting");

	printf("\nsceNetAdhocTerm: %08x\n", sceNetAdhocTerm());
	printf("sceNetAdhocInit: %08x\n", sceNetAdhocInit());
	createSome("After AdhocTerm and AdhocInit");

	adhocctlLeaveGroup();
	printf("\nsceNetAdhocctlTerm: %08x\n", sceNetAdhocctlTerm());
	printf("sceNetAdhocTerm: %08x\n", sceNetAdhocTerm());
	printf("sceNetTerm: %08x\n", sceNetTerm());
	printf("sceNetInit: %08x\n", sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000));
	printf("sceNetAdhocInit: %08x\n", sceNetAdhocInit());
	if (!adhocctlStartup()) {
		adhocShutdown();
		return 1;
	}
	// The helpers' event counting doesn't survive the restart, so create the group by hand.
	sceNetAdhocctlCreate("SOCKIDS");
	sceKernelDelayThread(3000000);
	createSome("After a full net restart");
	sceNetAdhocctlDisconnect();
	sceKernelDelayThread(1000000);

	adhocShutdown();
	return 0;
}
