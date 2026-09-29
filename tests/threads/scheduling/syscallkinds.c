#include <common.h>
#include <pspthreadman.h>
#include <pspiofilemgr.h>
#include <pspmodulemgr.h>
#include <psputility.h>
#include <pspgu.h>
#include <pspge.h>
#include <pspsuspend.h>
#include <malloc.h>
#include <string.h>

// What kind of wait is a slow syscall? Main runs at 0x20, a better thread (0x10) keeps delaying
// 200us, and a worse one (0x30) spins. If the worse thread runs during the call, the caller really
// waited (on IO, the GE, ...). If only the better one does, it was busy on the CPU, preemptibly.
// If neither, it didn't let go of the CPU at all.

int sceAtracSetDataAndGetID(void *buf, SceSize bufsize);
int sceAtracReleaseAtracID(int atracID);

static volatile int stopThreads = 0;
static volatile int highWakes = 0;
static volatile u32 wakeTimes[256];
static volatile u32 lowSpins = 0;

static int highThread(SceSize argc, void *argp) {
	while (!stopThreads) {
		sceKernelDelayThread(200);
		wakeTimes[highWakes++ & 255] = sceKernelGetSystemTimeLow();
	}
	return 0;
}

static int lowThread(SceSize argc, void *argp) {
	while (!stopThreads) {
		lowSpins++;
	}
	return 0;
}

typedef int (*Call)(void);

static const char *timeBucket(u32 us) {
	static char buf[32];
	if (us < 100) {
		return "<100us";
	} else if (us < 1000) {
		sprintf(buf, "~%dus", (us + 50) / 100 * 100);
	} else {
		sprintf(buf, "~%dms", (us + 500) / 1000);
	}
	return buf;
}

static void classify(const char *title, Call prepare, Call call) {
	u32 best = 0xFFFFFFFF;
	int fewestWakes = 1000;
	int lowRan = 0;
	int result = 0;

	stopThreads = 0;
	SceUID high = sceKernelCreateThread("high", &highThread, 0x10, 0x1000, 0, NULL);
	SceUID low = sceKernelCreateThread("low", &lowThread, 0x30, 0x1000, 0, NULL);
	sceKernelStartThread(high, 0, NULL);
	sceKernelStartThread(low, 0, NULL);

	for (int i = 0; i < 3; ++i) {
		if (prepare) {
			prepare();
		}
		int wakesBefore = highWakes;
		u32 spinsBefore = lowSpins;
		u32 start = sceKernelGetSystemTimeLow();
		result = call();
		u32 end = sceKernelGetSystemTimeLow();
		if (lowSpins != spinsBefore) {
			lowRan++;
		}
		int wakes = 0;
		for (int w = wakesBefore; w < highWakes; ++w) {
			if (wakeTimes[w & 255] - start < end - start - 10) {
				wakes++;
			}
		}
		fewestWakes = wakes < fewestWakes ? wakes : fewestWakes;
		best = end - start < best ? end - start : best;
	}

	// Stop them first: the worse thread would starve the output going to the host.
	stopThreads = 1;
	sceKernelTerminateDeleteThread(high);
	sceKernelTerminateDeleteThread(low);

	const char *kind;
	if (lowRan) {
		kind = "waits (worse thread ran)";
	} else if (fewestWakes > 0) {
		kind = "busy, preemptible (only the better thread ran)";
	} else {
		kind = "not preempted";
	}
	schedf("  %s: %s, result %s, %s\n", title, timeBucket(best), result < 0 ? "error" : "ok", kind);
	flushschedf();
}

// sceKernelLoadModule / sceKernelUnloadModule.
static const char *modulePath = "ms0:/_syscallkinds_module.prx";
static SceUID loadedModule = -1;

static int copyModule() {
	SceUID in = sceIoOpen("../../modules/mymodule.prx", PSP_O_RDONLY, 0);
	if (in < 0) {
		return in;
	}
	int size = sceIoLseek32(in, 0, PSP_SEEK_END);
	sceIoLseek32(in, 0, PSP_SEEK_SET);
	void *buf = malloc(size);
	sceIoRead(in, buf, size);
	sceIoClose(in);
	SceUID out = sceIoOpen(modulePath, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	sceIoWrite(out, buf, size);
	sceIoClose(out);
	free(buf);
	return 0;
}

static int loadModule() {
	loadedModule = sceKernelLoadModule(modulePath, 0, NULL);
	return loadedModule;
}

static int unloadLoaded() {
	if (loadedModule >= 0) {
		sceKernelUnloadModule(loadedModule);
		loadedModule = -1;
	}
	return 0;
}

static int loadThenUnloadPrepare() {
	return loadModule();
}

static int unloadModule() {
	int result = sceKernelUnloadModule(loadedModule);
	loadedModule = -1;
	return result;
}

// sceGeDrawSync, with the GE kept busy clearing the screen.
static unsigned int __attribute__((aligned(16))) list[262144];

static int queueClears() {
	sceGuStart(GU_DIRECT, list);
	for (int i = 0; i < 60; ++i) {
		sceGuClearColor(0xFF000000 | (i * 4));
		sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
	}
	sceGuFinish();
	return 0;
}

static int drawSync() {
	return sceGeDrawSync(0);
}

// sceKernelVolatileMemTryLock.
static int volatileTryLock() {
	void *ptr;
	int size;
	int result = sceKernelVolatileMemTryLock(0, &ptr, &size);
	if (result == 0) {
		sceKernelVolatileMemUnlock(0);
	}
	return result;
}

// sceAtracSetDataAndGetID.
static void *at3Data = NULL;
static int at3Size = 0;

static int atracSetData() {
	int id = sceAtracSetDataAndGetID(at3Data, at3Size);
	if (id >= 0) {
		sceAtracReleaseAtracID(id);
	}
	return id;
}

int main(int argc, char *argv[]) {
	checkpointNext("Syscalls:");

	schedf("  (copy module: %08x)\n", copyModule());
	classify("sceKernelLoadModule", &unloadLoaded, &loadModule);
	classify("sceKernelUnloadModule", &loadThenUnloadPrepare, &unloadModule);
	unloadLoaded();
	sceIoRemove(modulePath);

	sceGuInit();
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888, (void *)0, 512);
	sceGuDispBuffer(480, 272, (void *)0x88000, 512);
	sceGuDepthBuffer((void *)0x110000, 512);
	sceGuScissor(0, 0, 480, 272);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFinish();
	sceGuSync(0, 0);
	classify("sceGeDrawSync", &queueClears, &drawSync);
	sceGuTerm();

	classify("sceKernelVolatileMemTryLock", NULL, &volatileTryLock);

	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_ATRAC3PLUS);
	SceUID at3 = sceIoOpen("../../audio/atrac/sample.at3", PSP_O_RDONLY, 0);
	if (at3 >= 0) {
		at3Size = sceIoLseek32(at3, 0, PSP_SEEK_END);
		sceIoLseek32(at3, 0, PSP_SEEK_SET);
		at3Data = malloc(at3Size);
		sceIoRead(at3, at3Data, at3Size);
		sceIoClose(at3);
		classify("sceAtracSetDataAndGetID", NULL, &atracSetData);
		free(at3Data);
	} else {
		schedf("  (no sample.at3: %08x)\n", at3);
	}
	sceUtilityUnloadModule(PSP_MODULE_AV_ATRAC3PLUS);
	sceUtilityUnloadModule(PSP_MODULE_AV_AVCODEC);
	flushschedf();
	return 0;
}
