// Do sceVideocodec's slow calls occupy the Media Engine? A high-priority thread runs __sceSasCore
// (which mixes on the ME) in a loop and records how long each call takes, while the main thread
// calls sceMpegCreate (sceVideocodecOpen/Init/GetVersion/SetMemory) and sceMpegDelete
// (sceVideocodecDelete). If those keep the ME busy, SAS calls in the same window stretch to match.
// Timing probe, not a pass/fail test.
#include <common.h>

#include <pspkernel.h>
#include <psputility.h>
#include <pspmpeg.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "../sascore/sascore.h"

extern unsigned int HAS_DISPLAY;

int sceMpegAvcResourceInit(int unknown);
int sceMpegAvcResourceFinish(void);
void *sceMpegAvcResourceGetAvcDecTopAddr(void);

#define MAX_CALLS 4096
#define VAG_BYTES 8192

static SasCore sasCore __attribute__((aligned(64)));
static u8 vag[VAG_BYTES] __attribute__((aligned(64)));
static short out[256 * 2 * 2] __attribute__((aligned(64)));
static u32 callStart[MAX_CALLS];
static int callUs[MAX_CALLS];
static volatile int numCalls;
static volatile int quit;

static int sasThread(SceSize args, void *argp) {
	while (!quit && numCalls < MAX_CALLS) {
		u32 t0 = sceKernelGetSystemTimeLow();
		__sceSasCore(&sasCore, out);
		callStart[numCalls] = t0;
		callUs[numCalls] = (int)(sceKernelGetSystemTimeLow() - t0);
		numCalls++;
		sceKernelDelayThread(500);
	}
	return 0;
}

static void report(const char *name, u32 from, u32 to) {
	int i, n = 0, maxUs = 0, sum = 0;
	for (i = 0; i < numCalls; i++) {
		// Calls that overlap the window at all.
		u32 end = callStart[i] + callUs[i];
		if ((int)(end - from) >= 0 && (int)(callStart[i] - to) <= 0) {
			n++;
			sum += callUs[i];
			if (callUs[i] > maxUs) maxUs = callUs[i];
		}
	}
	printf("%-26s %6d us: %3d SAS calls, avg %5d us, max %6d us\n", name, (int)(to - from), n, n ? sum / n : 0, maxUs);
}

int main(int argc, char *argv[]) {
	HAS_DISPLAY = 0;
	if (sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC) < 0 || sceUtilityLoadModule(PSP_MODULE_AV_SASCORE) < 0 ||
		sceUtilityLoadModule(PSP_MODULE_AV_MPEGBASE) < 0) {
		printf("Could not load the modules\n");
		return 1;
	}

	int i;
	for (i = 0; i < VAG_BYTES; i += 16) {
		int j;
		vag[i] = 0x1A;
		vag[i + 1] = i == 0 ? 6 : (i + 16 >= VAG_BYTES ? 3 : 2);
		for (j = 2; j < 16; j++) {
			vag[i + j] = (u8)((i * 31 + j * 17) * 2654435761u >> 24);
		}
	}
	sceKernelDcacheWritebackAll();
	memset(&sasCore, 0, sizeof(sasCore));
	__sceSasInit(&sasCore, 256, 32, 0, 44100);
	for (i = 0; i < 8; i++) {
		__sceSasSetVoice(&sasCore, i, vag, VAG_BYTES, 1);
		__sceSasSetPitch(&sasCore, i, 0x1000);
		__sceSasSetVolume(&sasCore, i, 0x800, 0x800, 0x800, 0x800);
		__sceSasSetADSR(&sasCore, i, 15, 0x40000000, 0, 0x7FFFFFFF, 0);
		__sceSasSetKeyOn(&sasCore, i);
	}

	SceUID thread = sceKernelCreateThread("sas", sasThread, 0x10, 0x4000, 0, NULL);
	sceKernelStartThread(thread, 0, NULL);

	u32 t[10];
	t[0] = sceKernelGetSystemTimeLow();
	sceKernelDelayThread(100000);
	t[1] = sceKernelGetSystemTimeLow();
	sceMpegInit();
	sceMpegAvcResourceInit(1);
	void *decTop = sceMpegAvcResourceGetAvcDecTopAddr();
	int size = sceMpegQueryMemSize(1);
	void *data = memalign(64, size);
	SceMpeg mpeg;
	t[2] = sceKernelGetSystemTimeLow();
	int created = sceMpegCreate(&mpeg, data, size, NULL, 512, 1, (int)decTop);
	t[3] = sceKernelGetSystemTimeLow();
	sceKernelDelayThread(50000);
	t[4] = sceKernelGetSystemTimeLow();
	sceMpegDelete(&mpeg);
	t[5] = sceKernelGetSystemTimeLow();
	sceKernelDelayThread(50000);
	t[6] = sceKernelGetSystemTimeLow();
	sceMpegAvcResourceFinish();
	sceMpegFinish();
	quit = 1;
	sceKernelWaitThreadEnd(thread, NULL);

	printf("sceMpegCreate %08x\n", created);
	report("idle", t[0], t[1]);
	report("sceMpegCreate", t[2], t[3]);
	report("between", t[3], t[4]);
	report("sceMpegDelete", t[4], t[5]);
	report("after", t[5], t[6]);
	return 0;
}
