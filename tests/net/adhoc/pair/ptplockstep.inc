// How fast two consoles can step a game in lockstep over PTP, the way Fate/Unlimited Codes does in a
// network match: every vblank, poll the connection without waiting; if the peer's message is there,
// receive it, take a step and send our own; then wait for the next vblank. Also the same with
// PtpFlush after each send, and without the vblank wait (the bare ping-pong). Both sides print steps
// per second and how long from sending a message until the peer's next one arrived. Not a regression
// test: it prints times. Built as ptplockstep_host and ptplockstep_client.

#include "pair.h"
#include "../timing.h"
#include <pspdisplay.h>

#define PTP_PORT 0x5600
#define MSG_SIZE 32

static u8 msg[MSG_SIZE];

static int pollRecv(int id) {
	AdhocPollSd sd;
	sd.id = id;
	sd.events = 2; // RECV
	sd.revents = 0;
	return sceNetAdhocPollSocket(&sd, 1, 0, 1) > 0 && (sd.revents & 2);
}

static void lockstep(int ptp, const char *title, int waitVblank, int flush, int seconds) {
	Samples s = { 0 };
	int steps = 0, polls = 0, idlePolls = 0;
	u64 sentAt = timeNow();
	int len = MSG_SIZE;
	// The host sends first, the client answers: then both send one message per step.
#if HOST
	sceNetAdhocPtpSend(ptp, msg, &len, 0, 1);
	if (flush) {
		sceNetAdhocPtpFlush(ptp, 0, 1);
	}
#endif
	u64 start = timeNow();
	u64 end = start + seconds * 1000000ULL;
	while (timeNow() < end) {
		polls++;
		if (pollRecv(ptp)) {
			len = MSG_SIZE;
			if (sceNetAdhocPtpRecv(ptp, msg, &len, 0, 1) == 0 && len > 0) {
				sampleAdd(&s, timeNow() - sentAt);
				steps++;
				len = MSG_SIZE;
				sceNetAdhocPtpSend(ptp, msg, &len, 0, 1);
				if (flush) {
					sceNetAdhocPtpFlush(ptp, 0, 1);
				}
				sentAt = timeNow();
			}
		} else {
			idlePolls++;
		}
		if (waitVblank) {
			sceDisplayWaitVblankStartCB();
		} else {
			sceKernelDelayThread(100);
		}
	}
	printf("\n%s:\n", title);
	printf("  Steps per second: %d (%d of %d polls found nothing)\n", steps / seconds, idlePolls, polls);
	samplePrint("Send -> the peer's next message", &s);
	// Let the last messages drain before the next part.
	sceKernelDelayThread(500000);
	for (int i = 0; i < 100; ++i) {
		len = MSG_SIZE;
		if (sceNetAdhocPtpRecv(ptp, msg, &len, 0, 1) != 0) {
			break;
		}
	}
}

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	// Fate/Unlimited Codes's settings: 5776 byte buffer, rexmt 240 ms x 25.
#if HOST
	int listener = sceNetAdhocPtpListen(&localMac, PTP_PORT, 5776, 240000, 25, 1, 0);
	pairSync(1);
	int ptp = sceNetAdhocPtpAccept(listener, NULL, NULL, 5000000, 0);
#else
	pairSync(1);
	int ptp = sceNetAdhocPtpOpen(&localMac, 0, &peerMac, PTP_PORT, 5776, 240000, 25, 0);
	sceNetAdhocPtpConnect(ptp, 5000000, 0);
#endif
	printf("PTP: %s\n", ptp > 0 ? "connected" : "failed");
	pairSync(2);
	lockstep(ptp, "Poll, receive, send, wait for vblank", 1, 0, 10);
	pairSync(3);
	lockstep(ptp, "The same with PtpFlush after each send", 1, 1, 10);
	pairSync(4);
	lockstep(ptp, "Without waiting for vblank", 0, 0, 5);
	pairSync(5);
	sceNetAdhocPtpClose(ptp, 0);
#if HOST
	sceNetAdhocPtpClose(listener, 0);
#endif
	pairShutdown();
	return 0;
}
