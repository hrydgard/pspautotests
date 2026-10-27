// Ad hoc and infrastructure at the same time: what sceNetApctlConnect does while the console is in
// an ad hoc group, and what adhocctl does while apctl is connected.
//
// The result depends on the console's network settings, so this isn't in test.py. The .expected
// was recorded with profile 1 set up for an access point in range, with no security. Without a
// profile, apctl's Connect fails with 0x80110601 (BAD_NETCONF) instead.
//
// It also depends on the console. The .expected comes from a 6.60 one. On a 6.61 one, apctl can't
// join the access point at all once sceNetAdhocctlInit has been called, even with no group: it goes
// from JOINING back to DISCONNECTED with an ERROR event carrying 0x80410D09.

#include "adhoc.h"

int sceNetInetInit(void);
int sceNetInetTerm(void);
int sceNetApctlInit(int stacksize, int prio);
int sceNetApctlTerm(void);
int sceNetApctlConnect(int config);
int sceNetApctlDisconnect(void);
int sceNetApctlGetState(int *state);

// Getting an IP depends on how fast the access point's DHCP answers, so only wait until apctl is
// associated (GETTING_IP), which is what matters here.
#define APCTL_GETTING_IP 3

static int apctlState() {
	int state = -1;
	int result = sceNetApctlGetState(&state);
	return result < 0 ? result : state;
}

static int adhocctlState() {
	int state = -1;
	int result = sceNetAdhocctlGetState(&state);
	return result < 0 ? result : state;
}

// Polls apctl's state for up to timeoutMs, printing each state it passes through, and stops early
// once it reaches stopAt or has stayed in one state for 3 seconds.
static int followApctl(const char *title, int stopAt, int timeoutMs) {
	int last = -100;
	int sameFor = 0;
	printf("%s: apctl states", title);
	for (int i = 0; i < timeoutMs / 50; ++i) {
		int now = apctlState();
		if (now != last) {
			printf(" %d", now);
			last = now;
			sameFor = 0;
		} else {
			sameFor++;
		}
		if (now == stopAt || sameFor >= 60) {
			break;
		}
		sceKernelDelayThread(50000);
	}
	printf("\n");
	return last;
}

static void apctlDisconnect() {
	int result = sceNetApctlDisconnect();
	int state = followApctl("    Disconnect", 0, 5000);
	printf("    sceNetApctlDisconnect: %08x, ends in %d\n", result, state);
}

int main(int argc, char *argv[]) {
	if (!adhocStartup()) {
		return 1;
	}
	printf("load inet: %08x\n", sceUtilityLoadNetModule(PSP_NET_MODULE_INET));
	printf("sceNetInetInit: %08x\n", sceNetInetInit());
	printf("sceNetApctlInit: %08x\n", sceNetApctlInit(0x1600, 0x30));
	printf("sceNetAdhocInit: %08x\n", sceNetAdhocInit());
	if (!adhocctlStartup()) {
		adhocShutdown();
		return 1;
	}
	printf("apctl state %d, adhocctl state %d\n", apctlState(), adhocctlState());

	printf("\nApctl connect alone:\n");
	printf("  sceNetApctlConnect: %08x\n", sceNetApctlConnect(1));
	int state = followApctl("  Connecting", APCTL_GETTING_IP, 15000);
	printf("  Ends in %d, adhocctl state %d\n", state, adhocctlState());
	apctlDisconnect();

	printf("\nApctl connect while in an ad hoc group:\n");
	if (adhocctlCreateGroup("APCTLTST")) {
		printf("  sceNetApctlConnect: %08x\n", sceNetApctlConnect(1));
		state = followApctl("  Connecting", APCTL_GETTING_IP, 15000);
		printf("  Ends in %d, adhocctl state %d\n", state, adhocctlState());
		apctlDisconnect();
		printf("  adhocctl state afterwards %d\n", adhocctlState());
		adhocctlLeaveGroup();
	}

	printf("\nAdhocctl create while apctl is connected:\n");
	printf("  sceNetApctlConnect: %08x\n", sceNetApctlConnect(1));
	state = followApctl("  Connecting", APCTL_GETTING_IP, 15000);
	printf("  Ends in %d\n", state);
	int start = eventCount;
	int result = sceNetAdhocctlCreate("APCTLTST");
	int event = result < 0 ? -1 : waitEvent(start, 5000);
	printf("  sceNetAdhocctlCreate: %08x, event %d (error %08x), adhocctl state %d, apctl state %d\n", result, event,
		event >= 0 ? lastError : 0, adhocctlState(), apctlState());
	if (adhocctlState() > 0) {
		adhocctlLeaveGroup();
	}
	printf("  apctl state afterwards %d\n", apctlState());
	apctlDisconnect();

	// Shut down in the reverse order of startup, saying what's next, since a hang here is easy to miss.
	printf("\n");
	printf("sceNetAdhocctlTerm: %08x\n", sceNetAdhocctlTerm());
	printf("sceNetAdhocTerm: %08x\n", sceNetAdhocTerm());
	printf("sceNetApctlTerm: %08x\n", sceNetApctlTerm());
	printf("sceNetInetTerm: %08x\n", sceNetInetTerm());
	printf("sceNetTerm: %08x\n", sceNetTerm());
	printf("unload inet: %08x\n", sceUtilityUnloadNetModule(PSP_NET_MODULE_INET));
	printf("unload adhoc: %08x\n", sceUtilityUnloadNetModule(PSP_NET_MODULE_ADHOC));
	printf("unload common: %08x\n", sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON));
	return 0;
}
