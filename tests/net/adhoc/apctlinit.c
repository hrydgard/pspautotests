// sceNetApctlGetState and sceNetGetLocalEtherAddr around sceNetApctlInit and Term, without connecting.
// Some games (PES 2009) call GetState first and only call Init when it fails.

#include "adhoc.h"

int sceNetInetInit(void);
int sceNetInetTerm(void);
int sceNetApctlInit(int stacksize, int prio);
int sceNetApctlTerm(void);
int sceNetApctlGetState(int *state);

static void getState(const char *title) {
	int state = -1;
	int result = sceNetApctlGetState(&state);
	printf("%s: sceNetApctlGetState: %08x, state %d, NULL: %08x\n", title, result, state, sceNetApctlGetState(NULL));
}

static void localEtherAddr(const char *title) {
	AdhocMac mac;
	memset(&mac, 0xCC, sizeof(mac));
	int result = sceNetGetLocalEtherAddr(&mac);
	printf("%s: sceNetGetLocalEtherAddr: %08x, %s\n", title, result, mac.data[0] == 0xCC && mac.data[5] == 0xCC ? "untouched" : "written");
}

int main(int argc, char *argv[]) {
	if (!adhocStartup()) {
		return 1;
	}
	printf("load inet: %08x\n", sceUtilityLoadNetModule(PSP_NET_MODULE_INET));
	getState("Before InetInit");
	printf("sceNetInetInit: %08x\n", sceNetInetInit());
	getState("Before Init");
	localEtherAddr("Before Init");

	printf("sceNetApctlInit: %08x\n", sceNetApctlInit(0x1600, 0x30));
	getState("After Init");
	localEtherAddr("After Init");
	printf("sceNetApctlInit again: %08x\n", sceNetApctlInit(0x1600, 0x30));
	getState("After second Init");

	printf("sceNetApctlTerm: %08x\n", sceNetApctlTerm());
	getState("After Term");
	localEtherAddr("After Term");
	printf("sceNetApctlTerm again: %08x\n", sceNetApctlTerm());

	printf("sceNetApctlInit after Term: %08x\n", sceNetApctlInit(0x1600, 0x30));
	getState("After Init after Term");
	printf("sceNetApctlTerm: %08x\n", sceNetApctlTerm());

	printf("sceNetInetTerm: %08x\n", sceNetInetTerm());
	printf("sceNetTerm: %08x\n", sceNetTerm());
	printf("unload inet: %08x\n", sceUtilityUnloadNetModule(PSP_NET_MODULE_INET));
	printf("unload adhoc: %08x\n", sceUtilityUnloadNetModule(PSP_NET_MODULE_ADHOC));
	printf("unload common: %08x\n", sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON));
	return 0;
}
