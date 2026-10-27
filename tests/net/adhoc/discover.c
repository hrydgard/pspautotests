// sceNetAdhocDiscover on one console with nobody else around: the call sequence a game goes
// through, the status it reports, and what adhocctl's state does meanwhile.

#include "adhoc.h"

typedef struct {
	u32 unknown1;
	char group[8];
	u32 unknown2;
	unsigned int result;
} DiscoverParam;

int sceNetAdhocDiscoverInitStart(DiscoverParam *param);
int sceNetAdhocDiscoverUpdate(void);
int sceNetAdhocDiscoverGetStatus(void);
int sceNetAdhocDiscoverTerm(void);
int sceNetAdhocDiscoverStop(void);
int sceNetAdhocDiscoverRequestSuspend(void);

static int adhocctlState() {
	int state = -1;
	int result = sceNetAdhocctlGetState(&state);
	return result < 0 ? result : state;
}

// Calls Update every 16ms for up to timeoutMs, printing each status and adhocctl state change.
static int runDiscover(int timeoutMs) {
	int lastStatus = -100, lastState = -100;
	int status = 0;
	printf("  status/state:");
	for (int i = 0; i < timeoutMs / 16; ++i) {
		int update = sceNetAdhocDiscoverUpdate();
		status = sceNetAdhocDiscoverGetStatus();
		int state = adhocctlState();
		if (status != lastStatus || state != lastState) {
			printf(" %d/%d", status, state);
			if (update != 0) {
				printf("(update %08x)", update);
			}
			lastStatus = status;
			lastState = state;
		}
		if (status == 2) {
			break;
		}
		sceKernelDelayThread(16000);
	}
	printf("\n");
	return status;
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
	handlerId = sceNetAdhocctlAddHandler(&adhocctlHandler, NULL);

	printf("\nBefore InitStart:\n");
	printf("  GetStatus: %08x\n", sceNetAdhocDiscoverGetStatus());
	printf("  Update: %08x\n", sceNetAdhocDiscoverUpdate());
	printf("  Stop: %08x\n", sceNetAdhocDiscoverStop());
	printf("  Term: %08x\n", sceNetAdhocDiscoverTerm());
	// InitStart(NULL) hangs the console, so it isn't tried.

	printf("\nDiscovering, nobody around:\n");
	DiscoverParam param = { 0, { 'D', 'I', 'S', 'C', 'O', 'V', 'E', 'R' }, 0x14, 0xCCCC };
	int start = eventCount;
	printf("  InitStart: %08x, result field %x\n", sceNetAdhocDiscoverInitStart(&param), param.result);
	printf("  InitStart again: %08x\n", sceNetAdhocDiscoverInitStart(&param));
	int status = runDiscover(10000);
	printf("  Final status %d, result field %x\n", status, param.result);
	if (status != 2) {
		printf("  Stop: %08x\n", sceNetAdhocDiscoverStop());
		status = runDiscover(5000);
		printf("  After Stop: status %d, result field %x\n", status, param.result);
	}
	printf("  Handler events:");
	for (int i = start; i < eventCount && i < 16; ++i) {
		printf(" %d(%08x)", eventLog[i][0], eventLog[i][1]);
	}
	printf("\n");
	printf("  Term: %08x, status %d, adhocctl state %d\n", sceNetAdhocDiscoverTerm(), sceNetAdhocDiscoverGetStatus(), adhocctlState());

	printf("\nRequestSuspend:\n");
	param.result = 0xCCCC;
	start = eventCount;
	printf("  InitStart: %08x\n", sceNetAdhocDiscoverInitStart(&param));
	sceKernelDelayThread(500000);
	printf("  RequestSuspend: %08x\n", sceNetAdhocDiscoverRequestSuspend());
	status = runDiscover(5000);
	printf("  Final status %d, result field %x\n", status, param.result);
	if (status != 2) {
		sceNetAdhocDiscoverStop();
		runDiscover(5000);
	}
	printf("  Term: %08x\n", sceNetAdhocDiscoverTerm());

	printf("\n");
	adhocShutdown();
	return 0;
}
