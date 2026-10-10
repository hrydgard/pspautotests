// What becomes of a module's memory when it's unloaded: its text (which includes rodata), data and
// bss. Every word is compared with what was there while it was loaded.

#include <common.h>
#include <pspmodulemgr.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>

static char childPath[MAXPATHLEN];

typedef struct {
	const char *name;
	unsigned int addr;
	unsigned int size;
	unsigned int *before;
} Region;

static void snapshot(Region *r) {
	r->before = malloc(r->size);
	memcpy(r->before, (void *)r->addr, r->size);
}

static void classify(const Region *r) {
	int unchanged = 0, brk = 0, ff = 0, zero = 0, other = 0;
	unsigned int firstOther = 0;
	const unsigned int *now = (const unsigned int *)r->addr;
	unsigned int i;
	for (i = 0; i < r->size / 4; i++) {
		if (now[i] == r->before[i]) {
			unchanged++;
		} else if (now[i] == 0x0000004D) {  // break 1
			brk++;
		} else if (now[i] == 0xFFFFFFFF) {
			ff++;
		} else if (now[i] == 0) {
			zero++;
		} else {
			if (other == 0) {
				firstOther = now[i];
			}
			other++;
		}
	}
	printf("    %s: %d words, unchanged %d, break %d, ffffffff %d, zero %d, other %d", r->name, r->size / 4, unchanged, brk, ff, zero, other);
	if (other) {
		printf(" (first %08x)", firstOther);
	}
	printf("\n");
}

int main(int argc, char *argv[]) {
	getcwd(childPath, MAXPATHLEN);
	strcat(childPath, "/child.prx");

	SceUID mod = sceKernelLoadModule(childPath, 0, NULL);
	if (mod < 0) {
		printf("load failed: %08x\n", mod);
		return 1;
	}
	int status = 0;
	int result = sceKernelStartModule(mod, 0, NULL, &status, NULL);
	if (result < 0) {
		printf("start failed: %08x\n", result);
		return 1;
	}

	SceKernelModuleInfo info;
	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	result = sceKernelQueryModuleInfo(mod, &info);
	if (result < 0) {
		printf("query failed: %08x\n", result);
		return 1;
	}

	Region regions[3] = {
		{ "text", info.text_addr, info.text_size },
		{ "data", info.text_addr + info.text_size, info.data_size },
		{ "bss", info.text_addr + info.text_size + info.data_size, info.bss_size },
	};
	int i;
	for (i = 0; i < 3; i++) {
		snapshot(&regions[i]);
	}

	result = sceKernelStopModule(mod, 0, NULL, &status, NULL);
	printf("After stop (%08x):\n", result);
	for (i = 0; i < 3; i++) {
		classify(&regions[i]);
	}

	result = sceKernelUnloadModule(mod);
	printf("After unload (%08x):\n", result < 0 ? result : 0);
	for (i = 0; i < 3; i++) {
		classify(&regions[i]);
	}

	return 0;
}
