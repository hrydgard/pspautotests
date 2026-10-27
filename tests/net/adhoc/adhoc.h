#pragma once

#include <common.h>
#include <pspkernel.h>
#include <psputility.h>
#include <psputility_netmodules.h>
#include <string.h>

// Declared here, matching adhoc-imports.S, instead of using the SDK's headers, whose names and
// argument types don't match the firmware well.

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	u8 data[6];
} AdhocMac;

typedef struct {
	int type;
	char product[9];
	u8 padding[3];
} AdhocId;

typedef struct PdpStat {
	struct PdpStat *next;
	int id;
	AdhocMac mac;
	u16 port;
	u32 rcv;
} PdpStat;

typedef struct PtpStat {
	struct PtpStat *next;
	int id;
	AdhocMac mac;
	AdhocMac peermac;
	u16 port;
	u16 peerport;
	u32 snd;
	u32 rcv;
	int state;
} PtpStat;

typedef struct {
	int id;
	int events;
	int revents;
} AdhocPollSd;

typedef struct {
	int channel;
	char group[8];
	char nickname[128];
	AdhocMac bssid;
	u8 padding[2];
} AdhocctlParameter;

typedef struct AdhocctlPeerInfo {
	struct AdhocctlPeerInfo *next;
	char nickname[128];
	AdhocMac mac;
	u8 padding[6];
	u64 last_recv;
} AdhocctlPeerInfo;

typedef struct AdhocctlScanInfo {
	struct AdhocctlScanInfo *next;
	int channel;
	char group[8];
	AdhocMac bssid;
	u8 padding[2];
	int mode;
} AdhocctlScanInfo;

typedef void (*AdhocctlHandler)(int event, int error, void *arg);
typedef void (*MatchingHandler)(int id, int event, AdhocMac *mac, int optlen, void *opt);

int sceNetInit(int poolsize, int calloutprio, int calloutstack, int netintrprio, int netintrstack);
int sceNetTerm(void);
int sceNetGetLocalEtherAddr(AdhocMac *mac);
int sceWlanGetEtherAddr(AdhocMac *mac);
int sceWlanGetSwitchState(void);

int sceNetAdhocInit(void);
int sceNetAdhocTerm(void);
int sceNetAdhocPdpCreate(const AdhocMac *mac, u16 port, int bufsize, int flag);
int sceNetAdhocPdpSend(int id, const AdhocMac *dst, u16 port, const void *data, int len, u32 timeout, int flag);
int sceNetAdhocPdpRecv(int id, AdhocMac *src, u16 *port, void *buf, int *len, u32 timeout, int flag);
int sceNetAdhocPdpDelete(int id, int flag);
int sceNetAdhocGetPdpStat(int *size, PdpStat *stat);
int sceNetAdhocPtpOpen(const AdhocMac *mac, u16 port, const AdhocMac *dst, u16 dport, int bufsize, u32 rexmt_int, int rexmt_cnt, int flag);
int sceNetAdhocPtpConnect(int id, u32 timeout, int flag);
int sceNetAdhocPtpListen(const AdhocMac *mac, u16 port, int bufsize, u32 rexmt_int, int rexmt_cnt, int backlog, int flag);
int sceNetAdhocPtpAccept(int id, AdhocMac *mac, u16 *port, u32 timeout, int flag);
int sceNetAdhocPtpSend(int id, const void *data, int *len, u32 timeout, int flag);
int sceNetAdhocPtpRecv(int id, void *buf, int *len, u32 timeout, int flag);
int sceNetAdhocPtpFlush(int id, u32 timeout, int flag);
int sceNetAdhocPtpClose(int id, int flag);
int sceNetAdhocGetPtpStat(int *size, PtpStat *stat);
int sceNetAdhocSetSocketAlert(int id, int flag);
int sceNetAdhocGetSocketAlert(int id, int *flag);
int sceNetAdhocPollSocket(AdhocPollSd *sds, int count, u32 timeout, int flag);

int sceNetAdhocctlInit(int stacksize, int prio, const AdhocId *id);
int sceNetAdhocctlTerm(void);
int sceNetAdhocctlAddHandler(AdhocctlHandler handler, void *arg);
int sceNetAdhocctlDelHandler(int id);
int sceNetAdhocctlConnect(const char *group);
int sceNetAdhocctlCreate(const char *group);
int sceNetAdhocctlDisconnect(void);
int sceNetAdhocctlScan(void);
int sceNetAdhocctlGetState(int *state);
int sceNetAdhocctlGetParameter(AdhocctlParameter *param);
int sceNetAdhocctlGetScanInfo(int *size, AdhocctlScanInfo *info);
int sceNetAdhocctlGetPeerList(int *size, AdhocctlPeerInfo *info);
int sceNetAdhocctlGetPeerInfo(const AdhocMac *mac, int size, AdhocctlPeerInfo *info);
int sceNetAdhocctlGetAdhocId(AdhocId *id);
int sceNetAdhocctlJoin(const AdhocctlScanInfo *info);
int sceNetAdhocctlGetNameByAddr(const AdhocMac *mac, char *nickname);
int sceNetAdhocctlGetAddrByName(const char *nickname, int *size, AdhocctlPeerInfo *buf);
int sceNetAdhocctlCreateEnterGameMode(const char *group, int type, int num, const AdhocMac *macs, u32 timeout, int flag);
int sceNetAdhocctlCreateEnterGameModeMin(const char *group, int type, int min, int num, const AdhocMac *macs, u32 timeout, int flag);
int sceNetAdhocctlJoinEnterGameMode(const char *group, const AdhocMac *master, u32 timeout, int flag);
int sceNetAdhocctlExitGameMode(void);
int sceNetAdhocctlGetGameModeInfo(void *info);

int sceNetAdhocGameModeCreateMaster(const void *data, int size);
int sceNetAdhocGameModeCreateReplica(const AdhocMac *mac, void *data, int size);
int sceNetAdhocGameModeUpdateMaster(void);
int sceNetAdhocGameModeUpdateReplica(int id, void *info);
int sceNetAdhocGameModeDeleteMaster(void);
int sceNetAdhocGameModeDeleteReplica(int id);

int sceNetAdhocMatchingInit(int poolsize);
int sceNetAdhocMatchingTerm(void);
int sceNetAdhocMatchingCreate(int mode, int maxnum, u16 port, int rxbuflen, u32 hello_int, u32 keepalive_int, int init_count, u32 rexmt_int, MatchingHandler handler);
int sceNetAdhocMatchingStart(int id, int evthPri, int evthStack, int inthPri, int inthStack, int optlen, const void *opt);
int sceNetAdhocMatchingStart2(int id, int evthPri, int evthStack, int evthPartition, int inthPri, int inthStack, int inthPartition, int optlen, const void *opt);
int sceNetAdhocMatchingStop(int id);
int sceNetAdhocMatchingDelete(int id);
int sceNetAdhocMatchingSelectTarget(int id, const AdhocMac *mac, int optlen, const void *opt);
int sceNetAdhocMatchingCancelTarget(int id, const AdhocMac *mac);
int sceNetAdhocMatchingCancelTargetWithOpt(int id, const AdhocMac *mac, int optlen, const void *opt);
int sceNetAdhocMatchingAbortSendData(int id, const AdhocMac *mac);
int sceNetAdhocMatchingGetHelloOpt(int id, int *optlen, void *opt);
int sceNetAdhocMatchingSetHelloOpt(int id, int optlen, const void *opt);
int sceNetAdhocMatchingGetMembers(int id, int *size, void *buf);
int sceNetAdhocMatchingSendData(int id, const AdhocMac *mac, int len, const void *data);
int sceNetAdhocMatchingGetPoolMaxAlloc(void);
int sceNetAdhocMatchingGetPoolStat(int *stat);

#ifdef __cplusplus
}
#endif

static const AdhocMac MAC_ZERO = { { 0, 0, 0, 0, 0, 0 } };
static const AdhocMac MAC_BROADCAST = { { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } };
static const AdhocMac MAC_MULTICAST = { { 0x01, 0x00, 0x5E, 0x00, 0x00, 0x01 } };
// A unicast address that isn't any console in the room.
static const AdhocMac MAC_OTHER = { { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 } };

static AdhocMac localMac;

// Loads the modules and initializes sceNet and sceNetAdhoc. Returns false if any step failed.
static inline int adhocStartup() {
	setvbuf(stdout, NULL, _IONBF, 0);
	int result = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	if (result < 0) {
		printf("load common: %08x\n", result);
		return 0;
	}
	result = sceUtilityLoadNetModule(PSP_NET_MODULE_ADHOC);
	if (result < 0) {
		printf("load adhoc: %08x\n", result);
		return 0;
	}
	result = sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000);
	if (result < 0) {
		printf("sceNetInit: %08x\n", result);
		return 0;
	}
	return 1;
}

static const AdhocId TEST_ADHOC_ID = { 0, { 'T', 'E', 'S', 'T', '0', '0', '0', '0', '1' }, { 0, 0, 0 } };

// The local MAC address is only available once adhocctl is initialized.
static inline int adhocctlStartup() {
	int result = sceNetAdhocctlInit(0x2000, 0x30, &TEST_ADHOC_ID);
	printf("sceNetAdhocctlInit: %08x\n", result);
	if (result < 0) {
		return 0;
	}
	// sceNetGetLocalEtherAddr fails until a scan or group operation has started.
	result = sceWlanGetEtherAddr(&localMac);
	printf("sceWlanGetEtherAddr: %08x, switch %d\n", result, sceWlanGetSwitchState());
	return result >= 0;
}

static volatile int lastEvent = -1;
static volatile int lastError = 0;
static volatile int eventCount = 0;

static volatile int eventLog[16][2];

static void adhocctlHandler(int event, int error, void *arg) {
	lastEvent = event;
	lastError = error;
	if (eventCount < 16) {
		eventLog[eventCount][0] = event;
		eventLog[eventCount][1] = error;
	}
	eventCount++;
}

// Waits up to timeoutMs for the handler to see another event. Returns the event, or -1.
static inline int waitEvent(int startCount, int timeoutMs) {
	for (int i = 0; i < timeoutMs / 10; ++i) {
		if (eventCount != startCount) {
			return lastEvent;
		}
		sceKernelDelayThread(10000);
	}
	return -1;
}

static int handlerId = -1;

// Creates a group alone and waits for it to come up. Returns true once connected.
static inline int adhocctlCreateGroup(const char *name) {
	if (handlerId < 0) {
		handlerId = sceNetAdhocctlAddHandler(&adhocctlHandler, NULL);
	}
	int start = eventCount;
	int result = sceNetAdhocctlCreate(name);
	int event = result < 0 ? -1 : waitEvent(start, 5000);
	int state = -1;
	sceNetAdhocctlGetState(&state);
	printf("Create group: %08x, event %d (error %08x), state %d\n", result, event, event >= 0 ? lastError : 0, state);
	return event == 1;
}

static inline void adhocctlLeaveGroup() {
	int start = eventCount;
	int result = sceNetAdhocctlDisconnect();
	int event = result < 0 ? -1 : waitEvent(start, 5000);
	int state = -1;
	sceNetAdhocctlGetState(&state);
	printf("Leave group: %08x, event %d, state %d\n", result, event, state);
}

static inline void adhocShutdown() {
	sceNetAdhocctlTerm();
	sceNetAdhocTerm();
	sceNetTerm();
	sceUtilityUnloadNetModule(PSP_NET_MODULE_ADHOC);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
}

// MAC addresses differ between consoles, so print what an address is rather than the address.
static inline const char *macName(const AdhocMac *mac) {
	if (memcmp(mac, &localMac, sizeof(AdhocMac)) == 0)
		return "local";
	if (memcmp(mac, &MAC_ZERO, sizeof(AdhocMac)) == 0)
		return "zero";
	if (memcmp(mac, &MAC_BROADCAST, sizeof(AdhocMac)) == 0)
		return "broadcast";
	if (memcmp(mac, &MAC_OTHER, sizeof(AdhocMac)) == 0)
		return "other";
	return "unexpected";
}
