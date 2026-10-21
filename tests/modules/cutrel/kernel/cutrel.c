// ../cutrel's files loaded from kernel mode.
//
// Not loaded here: child_cut.prx and child_k_cut.prx, the cut modules with an export library. On a
// PSP-1000 (6.61, PSPLink) this load of child_cut.prx never returned and the PSP stopped answering,
// as from user mode. (PSPLink's modload refused PMDVIS's kcall.prx, which has exports, with
// 80020148.)

#include <common.h>
#include <pspkernel.h>
#include <string.h>

// Run main after module_start returns (see common/kernelglue.c).
int kernel_main_in_thread = 1;

SceUID kernelLoadModule(const char *path, int flags, SceKernelLMOption *option);
int kernelUnloadModule(SceUID mod);

static void tryLoad(const char *name) {
	char path[256];
	strcpy(path, "host0:/tests/modules/cutrel/");
	strcat(path, name);

	SceUID mod = kernelLoadModule(path, 0, NULL);
	if (mod < 0) {
		printf("%s: load %08x\n", name, mod);
		return;
	}
	printf("%s: loaded\n", name);
	printf("%s: unload %s\n", name, kernelUnloadModule(mod) >= 0 ? "OK" : "failed");
}

int main(int argc, char *argv[]) {
	tryLoad("child.prx");
	tryLoad("child_k.prx");
	tryLoad("child_n.prx");
	tryLoad("child_n_cut.prx");
	return 0;
}
