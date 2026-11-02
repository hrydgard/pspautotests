// What a non-blocking sceNetAdhocPtpFlush returns while sent data is on its way. The client sends
// 16 to 16384 bytes (eight times each) and then calls PtpFlush without blocking every 200 us until it
// returns 0; the host reads the data at once, or (for 1024 and 8192) 300 ms later. Then a blocking flush against a late reader, and
// last a send, one non-blocking flush and a close straight after, with the host looking 200 ms later.
// Not a regression test: it prints times. Built as ptpflush_host and ptpflush_client.

#include "pair.h"
#include "../timing.h"

#define PTP_PORT 0x5800
#define LATE_US 300000

static u8 buf[16384];

static const int sizes[] = { 16, 256, 1024, 2048, 4096, 8192, 16384 };
#define SIZES (int)(sizeof(sizes) / sizeof(sizes[0]))
#define REPS 8

static int isLateSize(int size) {
	return size == 1024 || size == 8192;
}

#if HOST
static int recvAll(int id, int want) {
	int got = 0;
	while (got < want) {
		int len = sizeof(buf);
		if (sceNetAdhocPtpRecv(id, buf, &len, 2000000, 0) < 0) {
			return got;
		}
		got += len;
	}
	return got;
}
#else
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

static Samples flushed;
static int firstResult, otherResult, sndAfterSend, sndAfterFlush;

static void sendAndFlush(int c, int size) {
	memset(buf, 1, size);
	int len = size;
	sceNetAdhocPtpSend(c, buf, &len, 2000000, 0);
	sndAfterSend = ptpSnd(c);
	u64 t0 = timeNow();
	int result = sceNetAdhocPtpFlush(c, 0, 1);
	firstResult = result;
	while (result != 0 && timeNow() - t0 < 2000000) {
		sceKernelDelayThread(200);
		result = sceNetAdhocPtpFlush(c, 0, 1);
		if (result != 0 && result != firstResult) {
			otherResult = result;
		}
	}
	if (result == 0) {
		sampleAdd(&flushed, timeNow() - t0);
	}
	sndAfterFlush = ptpSnd(c);
}

static void printFlushes(int size, const char *reader) {
	char title[64];
	snprintf(title, sizeof(title), "%5d, reader %s: until 0", size, reader);
	samplePrint(title, &flushed);
}
#endif

int main(int argc, char *argv[]) {
	if (!pairStartup()) {
		pairShutdown();
		return 1;
	}
	int step = 10;
#if HOST
	int listener = sceNetAdhocPtpListen(&localMac, PTP_PORT, 0x8000, 200000, 5, 1, 0);
	pairSync(step++);
	int c = sceNetAdhocPtpAccept(listener, NULL, NULL, 3000000, 0);
	printf("Accept: %s\n", c > 0 ? "ok" : "failed");
	pairSync(step++);
	pairSync(step++);
	for (int late = 0; late < 2; ++late) {
		for (int z = 0; z < SIZES; ++z) {
			if (late && !isLateSize(sizes[z])) {
				continue;
			}
			int short_ = 0;
			for (int r = 0; r < REPS; ++r) {
				pairSync(step++);
				if (late) {
					sceKernelDelayThread(LATE_US);
				}
				if (recvAll(c, sizes[z]) != sizes[z]) {
					short_++;
				}
				pairSync(step++);
			}
			if (short_) {
				printf("Size %d: %d short receives\n", sizes[z], short_);
			}
		}
	}
	// Blocking flush against a late reader.
	pairSync(step++);
	sceKernelDelayThread(LATE_US);
	recvAll(c, 1024);
	pairSync(step++);
	// Send, flush once, close.
	pairSync(step++);
	sceKernelDelayThread(200000);
	AdhocPollSd sd;
	sd.id = c;
	sd.events = 0x1 | 0x2 | 0x200 | 0x800;
	sd.revents = 0;
	int pollResult = sceNetAdhocPollSocket(&sd, 1, 0, 1);
	printf("After the close: poll %08x, revents %04x\n", pollResult, sd.revents);
	int total = 0, recvResult = 0;
	for (int i = 0; i < 20; ++i) {
		int len = sizeof(buf);
		recvResult = sceNetAdhocPtpRecv(c, buf, &len, 500000, 0);
		if (recvResult < 0) {
			break;
		}
		total += len;
	}
	printf("  Received %d bytes, then %08x\n", total, recvResult);
	pairSync(step++);
	sceNetAdhocPtpClose(c, 0);
	sceNetAdhocPtpClose(listener, 0);
#else
	pairSync(step++);
	int c = sceNetAdhocPtpOpen(&localMac, 0, &peerMac, PTP_PORT, 0x8000, 200000, 5, 0);
	int connectResult = sceNetAdhocPtpConnect(c, 3000000, 0);
	printf("Connect: %08x\n", connectResult);
	pairSync(step++);
	printf("Flush with nothing sent: %08x\n", sceNetAdhocPtpFlush(c, 0, 1));
	pairSync(step++);
	printf("Non-blocking flush after a send:\n");
	for (int late = 0; late < 2; ++late) {
		for (int z = 0; z < SIZES; ++z) {
			if (late && !isLateSize(sizes[z])) {
				continue;
			}
			firstResult = otherResult = 0;
			int sndOk = 1;
			for (int r = 0; r < REPS; ++r) {
				pairSync(step++);
				sendAndFlush(c, sizes[z]);
				if (sndAfterSend != sizes[z] || sndAfterFlush != 0) {
					sndOk = 0;
				}
				pairSync(step++);
			}
			printf("  %5d: first flush %08x", sizes[z], firstResult);
			if (otherResult) {
				printf(", also %08x", otherResult);
			}
			printf(", snd stat the size after the send and 0 after the flush: %s\n", sndOk ? "yes" : "no");
			printFlushes(sizes[z], late ? "late" : "at once");
		}
	}
	pairSync(step++);
	memset(buf, 1, 1024);
	int len = 1024;
	sceNetAdhocPtpSend(c, buf, &len, 2000000, 0);
	u64 t0 = timeNow();
	int flushResult = sceNetAdhocPtpFlush(c, 2000000, 0);
	printf("Blocking flush after 1024, reader late: %08x after %u us\n", flushResult, (unsigned)(timeNow() - t0));
	pairSync(step++);
	pairSync(step++);
	memset(buf, 1, sizeof(buf));
	int sent = 0;
	for (int i = 0; i < 2; ++i) {
		len = 4096;
		if (sceNetAdhocPtpSend(c, buf, &len, 2000000, 0) == 0) {
			sent += len;
		}
	}
	flushResult = sceNetAdhocPtpFlush(c, 0, 1);
	printf("Sent %d, flush %08x, close %08x\n", sent, flushResult, sceNetAdhocPtpClose(c, 0));
	pairSync(step++);
#endif
	pairShutdown();
	return 0;
}
