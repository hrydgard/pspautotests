// Loading, from user mode, a PRX whose relocation sections lie past the end of the file (*_cut.prx,
// see child.mak). PMDVIS's kcall.prx is like this. Complete copies are the controls. Nothing is
// started. kernel/cutrel loads the same files from kernel mode.
//
// Not loaded here: child_cut.prx and child_k_cut.prx, cut modules with an export library besides
// syslib (child_k is a kernel module, as PMDVIS's is). On a PSP-1000 (6.61, PSPLink) loading
// either from user mode never returned, and the PSP stopped answering.

#include <common.h>
#include <pspmodulemgr.h>
#include <string.h>

// From psp-nm child.elf/child_n.elf: where valuePtr is, and the address of value it holds once
// relocated.
#define VALUEPTR_OFFSET 0x23C
#define VALUE_OFFSET 0x238
#define VALUEPTR_OFFSET_N 0x1F4
#define VALUE_OFFSET_N 0x1F0

static void tryLoad(const char *name, int kernel, unsigned int valuePtrOffset, unsigned int valueOffset) {
	char path[256];
	strcpy(path, "host0:/tests/modules/cutrel/");
	strcat(path, name);

	SceUID mod = sceKernelLoadModule(path, 0, NULL);
	if (mod < 0) {
		printf("%s: load %08x\n", name, mod);
		return;
	}
	// User code can't read a kernel module's memory.
	if (kernel) {
		printf("%s: loaded\n", name);
	} else {
		SceKernelModuleInfo info;
		memset(&info, 0, sizeof(info));
		info.size = sizeof(info);
		sceKernelQueryModuleInfo(mod, &info);
		unsigned int base = info.text_addr;
		unsigned int valuePtr = *(volatile unsigned int *)(base + valuePtrOffset);
		if (valuePtr == base + valueOffset) {
			printf("%s: loaded, relocated\n", name);
		} else if (valuePtr == valueOffset) {
			printf("%s: loaded, not relocated\n", name);
		} else {
			printf("%s: loaded, valuePtr = text + %08x\n", name, valuePtr - base);
		}
	}
	printf("%s: unload %s\n", name, sceKernelUnloadModule(mod) >= 0 ? "OK" : "failed");
}

int main(int argc, char *argv[]) {
	tryLoad("child.prx", 0, VALUEPTR_OFFSET, VALUE_OFFSET);
	tryLoad("child_k.prx", 1, 0, 0);
	tryLoad("child_n.prx", 0, VALUEPTR_OFFSET_N, VALUE_OFFSET_N);
	tryLoad("child_n_cut.prx", 0, VALUEPTR_OFFSET_N, VALUE_OFFSET_N);
	return 0;
}
