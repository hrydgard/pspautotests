// What a PTP sender sees while sent data is on its way to a receiver that reads it at once: whether
// PollSocket reports SEND and FLUSH right after a send, how long until it does, and what a non-blocking
// send of 16 more bytes returns then. 8K send buffer; 16, 4096 and 8192 bytes, five times each.
// Built as ptpsendready_host and ptpsendready_client.

#include "pair.h"
#include "../timing.h"

#define PTP_PORT 0x5900
#define REPS 5

static u8 buf[8192];

static const int sizes[] = { 16, 4096, 8192 };
#define SIZES (int)(sizeof(sizes) / sizeof(sizes[0]))

#if HOST
// Reads until nothing more comes for 300 ms.
static int drain(int id) {
	int got = 0;
	while (1) {
		int len = sizeof(buf);
		if (sceNetAdhocPtpRecv(id, buf, &len, 300000, 0) < 0) {
			return got;
		}
		got += len;
	}
}
#else
#define EV_SEND 0x01
#define EV_FLUSH 0x10

static int pollBits(int id) {
	AdhocPollSd sd;
	sd.id = id;
	sd.events = EV_SEND | EV_FLUSH;
	sd.revents = 0;
	sceNetAdhocPollSocket(&sd, 1, 0, 1);
	return sd.revents;
}

static int ptpSnd(int id) {
	static PtpStat stats[8];
	int size = sizeof(stats);
	if (sceNetAdhocGetPtpStat(&size, stats) < 0) {
		return -1;
	}
	for (int i = 0; i < size / (int)sizeof(PtpStat) && i < 8; ++i) {
		if (stats[i].id == id) {
			return stats[i].snd;
		}
	}
	return -2;
}
#endif

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	int step = 10;
#if HOST
	int listener = sceNetAdhocPtpListen(&localMac, PTP_PORT, 0x2000, 200000, 5, 1, 0);
	pairSync(step++);
	int c = sceNetAdhocPtpAccept(listener, NULL, NULL, 3000000, 0);
	printf("Accept: %s\n", c > 0 ? "ok" : "failed");
	pairSync(step++);
	for (int z = 0; z < SIZES; ++z) {
		int total = 0;
		for (int r = 0; r < REPS; ++r) {
			pairSync(step++);
			total += drain(c);
			pairSync(step++);
		}
		printf("%5d: received %d bytes in all\n", sizes[z], total);
	}
	sceNetAdhocPtpClose(c, 0);
	sceNetAdhocPtpClose(listener, 0);
#else
	pairSync(step++);
	int c = sceNetAdhocPtpOpen(&localMac, 0, &peerMac, PTP_PORT, 0x2000, 200000, 5, 0);
	printf("Connect: %08x\n", sceNetAdhocPtpConnect(c, 3000000, 0));
	pairSync(step++);
	memset(buf, 1, sizeof(buf));
	Samples sendReady = { 0 }, flushReady = { 0 };
	for (int z = 0; z < SIZES; ++z) {
		int sendNow = 0, flushNow = 0, extraOk = 0, extraOther = 0, never = 0, sndFull = 0;
		for (int r = 0; r < REPS; ++r) {
			pairSync(step++);
			int len = sizes[z];
			sceNetAdhocPtpSend(c, buf, &len, 2000000, 0);
			u64 t0 = timeNow();
			if (ptpSnd(c) == sizes[z]) {
				sndFull++;
			}
			int bits = pollBits(c);
			sendNow += (bits & EV_SEND) != 0;
			flushNow += (bits & EV_FLUSH) != 0;
			int extraLen = 16;
			int extra = sceNetAdhocPtpSend(c, buf, &extraLen, 0, 1);
			if (extra == 0 && extraLen == 16) {
				extraOk++;
			} else if (!extraOther) {
				extraOther = extra;
			}
			u64 sendAt = 0, flushAt = 0;
			while (timeNow() - t0 < 200000 && (!sendAt || !flushAt)) {
				bits = pollBits(c);
				u64 t = timeNow() - t0;
				if (!sendAt && (bits & EV_SEND)) {
					sendAt = t;
				}
				if (!flushAt && (bits & EV_FLUSH)) {
					flushAt = t;
				}
				sceKernelDelayThread(200);
			}
			if (sendAt) {
				sampleAdd(&sendReady, sendAt);
			}
			if (flushAt) {
				sampleAdd(&flushReady, flushAt);
			}
			if (!sendAt || !flushAt) {
				never++;
			}
			// Let it all arrive before the next round.
			sceNetAdhocPtpFlush(c, 2000000, 0);
			pairSync(step++);
		}
		printf("%5d: snd stat the size right after: %d/%d, poll right after: SEND %d/%d, FLUSH %d/%d\n",
			sizes[z], sndFull, REPS, sendNow, REPS, flushNow, REPS);
		printf("       non-blocking send of 16 then: ok %d/%d", extraOk, REPS);
		if (extraOther) {
			printf(", else %08x", extraOther);
		}
		printf(", SEND or FLUSH not within 200 ms: %d\n", never);
		char title[64];
		snprintf(title, sizeof(title), "%5d: poll SEND after", sizes[z]);
		samplePrint(title, &sendReady);
		snprintf(title, sizeof(title), "%5d: poll FLUSH after", sizes[z]);
		samplePrint(title, &flushReady);
	}
	sceNetAdhocPtpClose(c, 0);
#endif
	pairShutdown();
	return 0;
}
