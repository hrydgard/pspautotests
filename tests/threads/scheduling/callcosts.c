#include <common.h>
#include <pspthreadman.h>
#include <pspiofilemgr.h>
#include <pspmodulemgr.h>
#include <psputility.h>
#include <malloc.h>
#include <string.h>

// Where the time goes in two slow calls, sceKernelLoadModule and sceAtracSetDataAndGetID. Each
// piece is timed (best of three) alongside a worse thread (0x30) that spins, and its share of the
// CPU during the call says how much of that time the caller spent waiting rather than working.
// The ms0 times depend on the card: these were recorded with an SD card in a memory stick adapter.

int sceAtracGetAtracID(int codecType);
int sceAtracSetData(int atracID, void *buf, SceSize bufsize);
int sceAtracSetDataAndGetID(void *buf, SceSize bufsize);
int sceAtracSetHalfwayBufferAndGetID(void *buf, SceSize readSize, SceSize bufsize);
int sceAtracReleaseAtracID(int atracID);
int sceAtracDecodeData(int atracID, u16 *outSamples, int *outN, int *outEnd, int *outRemainFrame);

static volatile int stopProbe = 0;
static volatile u32 probeSpins = 0;
static u32 spinsPerMs = 0;

static int probeThread(SceSize argc, void *argp) {
	while (!stopProbe) {
		probeSpins++;
	}
	return 0;
}

static const char *timeBucket(u32 us) {
	static char buf[32];
	if (us < 100) {
		return "<100us";
	} else if (us < 1000) {
		sprintf(buf, "~%dus", (us + 50) / 100 * 100);
	} else if (us < 10000) {
		sprintf(buf, "~%d.%dms", (us + 250) / 1000, ((us + 250) % 1000) / 500 * 5);
	} else {
		sprintf(buf, "~%dms", (us + 500) / 1000);
	}
	return buf;
}

static const char *shareBucket(u32 spins, u32 us) {
	u32 possible = spinsPerMs * us / 1000;
	if (possible == 0) {
		return "?";
	}
	u32 percent = (u32)((u64)spins * 100 / possible);
	if (percent < 10) {
		return "under 10%";
	} else if (percent < 50) {
		return "10-50%";
	} else if (percent < 90) {
		return "50-90%";
	}
	return "over 90%";
}

typedef int (*Call)(void);

static void measure(const char *title, Call prepare, Call call, Call cleanup) {
	u32 best = 0xFFFFFFFF;
	u32 bestSpins = 0;
	int result = 0;
	for (int i = 0; i < 3; ++i) {
		if (prepare) {
			prepare();
		}
		u32 spinsBefore = probeSpins;
		u32 start = sceKernelGetSystemTimeLow();
		result = call();
		u32 end = sceKernelGetSystemTimeLow();
		u32 spins = probeSpins - spinsBefore;
		if (cleanup) {
			cleanup();
		}
		if (end - start < best) {
			best = end - start;
			bestSpins = spins;
		}
	}
	if (result < 0) {
		schedf("  %s: %s, worse thread got %s, error %08x\n", title, timeBucket(best), shareBucket(bestSpins, best), result);
	} else {
		schedf("  %s: %s, worse thread got %s, result ok\n", title, timeBucket(best), shareBucket(bestSpins, best));
	}
}

// Modules.
static const char *smallPath = "ms0:/_callcosts_small.prx";
static const char *largePath = "ms0:/_callcosts_large.prx";
static const char *path = NULL;
static SceUID fd = -1;
static SceUID module = -1;
static char fileBuf[256 * 1024];
static int fileSize = 0;

static int copyFile(const char *from, const char *to) {
	SceUID in = sceIoOpen(from, PSP_O_RDONLY, 0);
	if (in < 0) {
		return in;
	}
	int size = sceIoRead(in, fileBuf, sizeof(fileBuf));
	sceIoClose(in);
	SceUID out = sceIoOpen(to, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	sceIoWrite(out, fileBuf, size);
	sceIoClose(out);
	return size;
}

static int ioOpen() {
	fd = sceIoOpen(path, PSP_O_RDONLY, 0);
	return fd;
}

static int ioRead() {
	return sceIoRead(fd, fileBuf, fileSize);
}

static int ioClose() {
	int result = sceIoClose(fd);
	fd = -1;
	return result;
}

static int ioRewind() {
	return sceIoLseek32(fd, 0, PSP_SEEK_SET);
}

static int loadModule() {
	module = sceKernelLoadModule(path, 0, NULL);
	return module;
}

static int unloadModule() {
	if (module >= 0) {
		sceKernelUnloadModule(module);
	}
	module = -1;
	return 0;
}

static void measureModule(const char *name, const char *p, int size) {
	path = p;
	fileSize = size;
	schedf("  %s (%s):\n", name, size < 16 * 1024 ? "under 16KB" : "over 100KB");
	measure("  sceIoOpen", NULL, &ioOpen, &ioClose);
	ioOpen();
	measure("  sceIoRead (whole file)", &ioRewind, &ioRead, NULL);
	ioClose();
	measure("  sceIoClose", &ioOpen, &ioClose, NULL);
	measure("  sceKernelLoadModule", NULL, &loadModule, &unloadModule);
}

// Atrac.
static void *at3Data = NULL;
static int at3Size = 0;
static int atracID = -1;

static int loadAt3(const char *filename) {
	SceUID in = sceIoOpen(filename, PSP_O_RDONLY, 0);
	if (in < 0) {
		return in;
	}
	at3Size = sceIoLseek32(in, 0, PSP_SEEK_END);
	sceIoLseek32(in, 0, PSP_SEEK_SET);
	free(at3Data);
	at3Data = malloc(at3Size);
	sceIoRead(in, at3Data, at3Size);
	sceIoClose(in);
	return 0;
}

static int getID3() {
	atracID = sceAtracGetAtracID(0x1001);
	return atracID;
}

static int getID3Plus() {
	atracID = sceAtracGetAtracID(0x1000);
	return atracID;
}

static int setData() {
	return sceAtracSetData(atracID, at3Data, at3Size);
}

static int setDataAndGetID() {
	atracID = sceAtracSetDataAndGetID(at3Data, at3Size);
	return atracID;
}

static int setHalfwayAndGetID() {
	atracID = sceAtracSetHalfwayBufferAndGetID(at3Data, 0x1000, at3Size);
	return atracID;
}

static u16 __attribute__((aligned(64))) pcm[2048 * 2];

static int decode() {
	int samples = 0, end = 0, remain = 0;
	return sceAtracDecodeData(atracID, pcm, &samples, &end, &remain);
}

static int setDataAndDecode() {
	setDataAndGetID();
	return decode();
}

static int release() {
	int result = 0;
	if (atracID >= 0) {
		result = sceAtracReleaseAtracID(atracID);
	}
	atracID = -1;
	return result;
}

static void measureAtrac(const char *name, Call getID) {
	schedf("  %s:\n", name);
	measure("  sceAtracGetAtracID", NULL, getID, &release);
	measure("  sceAtracSetData", getID, &setData, &release);
	measure("  sceAtracSetDataAndGetID", NULL, &setDataAndGetID, &release);
	measure("  sceAtracSetHalfwayBufferAndGetID (4KB)", NULL, &setHalfwayAndGetID, &release);
	measure("  sceAtracDecodeData (first frame)", &setDataAndGetID, &decode, &release);
	measure("  sceAtracDecodeData (second frame)", &setDataAndDecode, &decode, &release);
	measure("  sceAtracReleaseAtracID", &setDataAndGetID, &release, NULL);
}

int main(int argc, char *argv[]) {
	checkpointNext("Costs:");

	SceUID probe = sceKernelCreateThread("probe", &probeThread, 0x30, 0x1000, 0, NULL);
	sceKernelStartThread(probe, 0, NULL);
	u32 before = probeSpins;
	sceKernelDelayThread(20000);
	spinsPerMs = (probeSpins - before) / 20;

	int smallSize = copyFile("../../modules/mymodule.prx", smallPath);
	int largeSize = copyFile("dispatch.prx", largePath);

	measureModule("Small module", smallPath, smallSize);
	measureModule("Large module", largePath, largeSize);
	sceIoRemove(smallPath);
	sceIoRemove(largePath);

	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_ATRAC3PLUS);
	if (loadAt3("../../audio/atrac/test_mono.at3") >= 0) {
		measureAtrac("ATRAC3, mono, 10KB", &getID3);
	}
	if (loadAt3("../../audio/atrac/sample.at3") >= 0) {
		measureAtrac("ATRAC3+, stereo, 45KB", &getID3Plus);
	}
	sceUtilityUnloadModule(PSP_MODULE_AV_ATRAC3PLUS);
	sceUtilityUnloadModule(PSP_MODULE_AV_AVCODEC);
	free(at3Data);

	// Stop it first: it would starve the output going to the host.
	stopProbe = 1;
	sceKernelTerminateDeleteThread(probe);
	flushschedf();
	return 0;
}
