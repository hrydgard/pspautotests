// When a module's contents reach memory, relative to sceKernelLoadModule returning. While the load
// is in progress, a higher priority thread writes a marker over the start of the lowest free block
// (where a low-placed module lands) through the uncached mirror, the way SOCOM: Fireteam Bravo 2's
// EU loader overruns its display list into the game module it is loading. Afterwards the start of
// the module's text is compared with a load nobody wrote over.

#include <common.h>
#include <pspmodulemgr.h>
#include <pspsysmem.h>
#include <pspthreadman.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>

#define WORDS 64
#define MARKER 0x5C5C0000

static char childPath[MAXPATHLEN];
static unsigned int probeAddr;
static unsigned int reference[WORDS];
static unsigned int referenceAddr;
static volatile int loading;
static volatile int scribbledDuringLoad;

static unsigned int lowestFree() {
	SceUID blk = sceKernelAllocPartitionMemory(2, "probe", PSP_SMEM_Low, 0x100, NULL);
	unsigned int addr = (unsigned int)sceKernelGetBlockHeadAddr(blk);
	sceKernelFreePartitionMemory(blk);
	return addr;
}

static int scribbler(SceSize args, void *argp) {
	sceKernelDelayThread(2000);
	scribbledDuringLoad = loading;
	volatile unsigned int *p = (volatile unsigned int *)(probeAddr | 0x40000000);
	int i;
	for (i = 0; i < WORDS; i++) {
		p[i] = MARKER + i;
	}
	return 0;
}

static SceUID load(SceKernelLMOption *opt) {
	loading = 1;
	SceUID mod = sceKernelLoadModule(childPath, 0, opt);
	loading = 0;
	return mod;
}

static unsigned int textAddr(SceUID mod) {
	SceKernelModuleInfo info;
	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	sceKernelQueryModuleInfo(mod, &info);
	return info.text_addr;
}

// Never started: in the race, module_start may have been written over.
static void unload(SceUID mod) {
	sceKernelUnloadModule(mod);
}

static void testRace(const char *title, SceKernelLMOption *opt) {
	printf("%s:\n", title);

	SceUID thread = sceKernelCreateThread("scribbler", &scribbler, 0x10, 0x1000, 0, NULL);
	probeAddr = lowestFree();
	scribbledDuringLoad = -1;
	sceKernelStartThread(thread, 0, NULL);

	SceUID mod = load(opt);
	if (mod < 0) {
		printf("  load failed: %08x\n", mod);
		sceKernelWaitThreadEnd(thread, NULL);
		sceKernelDeleteThread(thread);
		return;
	}
	sceKernelWaitThreadEnd(thread, NULL);
	sceKernelDeleteThread(thread);

	unsigned int addr = textAddr(mod);
	printf("  marker written %s\n", scribbledDuringLoad == 1 ? "during the load" : scribbledDuringLoad == 0 ? "after the load" : "never");
	printf("  text at the lowest free block: %s, same place as the reference: %s\n", addr == probeAddr ? "yes" : "no", addr == referenceAddr ? "yes" : "no");

	int module = 0, marker = 0, other = 0;
	const unsigned int *p = (const unsigned int *)addr;
	int i;
	for (i = 0; i < WORDS; i++) {
		if (p[i] == reference[i]) {
			module++;
		} else if (p[i] == MARKER + i) {
			marker++;
		} else {
			other++;
		}
	}
	printf("  first %d words of text: module %d, marker %d, other %d\n", WORDS, module, marker, other);
	unload(mod);
}

int main(int argc, char *argv[]) {
	getcwd(childPath, MAXPATHLEN);
	strcat(childPath, "/child.prx");

	// The reference: what the start of the text holds when nothing interferes.
	unsigned int freeAddr = lowestFree();
	SceUID mod = load(NULL);
	if (mod < 0) {
		printf("reference load failed: %08x\n", mod);
		return 1;
	}
	referenceAddr = textAddr(mod);
	memcpy(reference, (const void *)referenceAddr, sizeof(reference));
	printf("Reference: text at the lowest free block: %s\n", referenceAddr == freeAddr ? "yes" : "no");
	unload(mod);

	testRace("No options", NULL);

	// What SOCOM: Fireteam Bravo 2's loader passes.
	SceKernelLMOption opt;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.mpidtext = 2;
	opt.mpiddata = 2;
	opt.position = 0;
	testRace("User partition, low", &opt);

	return 0;
}
