#include <common.h>
#include <pspintrman.h>

// mfic/mtic $0 are the CPU's interrupt enable flag. usersystemlib's sceKernelCpuSuspendIntr is
// mfic v0, $0; mtic zero, $0, and sceKernelCpuResumeIntr is mtic a0, $0.

static inline u32 mfic() {
	// Preload the output so an mfic that doesn't write it shows up.
	u32 v = 0xDEADBEEF;
	asm volatile("mfic %0, $0" : "+r"(v));
	return v;
}

static inline void mtic(u32 v) {
	asm volatile("mtic %0, $0" : : "r"(v));
}

int main(int argc, char *argv[]) {
	unsigned int results[16];
	int n = 0;

	results[n++] = mfic();

	int state = sceKernelCpuSuspendIntr();
	results[n++] = mfic();
	sceKernelCpuResumeIntr(state);
	results[n++] = mfic();

	mtic(0);
	results[n++] = mfic();
	results[n++] = sceKernelIsCpuIntrEnable();
	state = sceKernelCpuSuspendIntr();
	results[n++] = state;
	mtic(1);
	results[n++] = mfic();
	results[n++] = sceKernelIsCpuIntrEnable();

	mtic(0);
	mtic(2);
	results[n++] = mfic();
	mtic(0);
	mtic(0x80000000);
	results[n++] = mfic();
	state = sceKernelCpuSuspendIntr();
	results[n++] = state;
	sceKernelCpuResumeIntr(1);
	results[n++] = mfic();
	sceKernelCpuResumeIntr(2);
	results[n++] = mfic();
	sceKernelCpuResumeIntr(1);

	int i = 0;
	printf("Initial: %08x\n", results[i++]);
	printf("After sceKernelCpuSuspendIntr: %08x\n", results[i++]);
	printf("After sceKernelCpuResumeIntr: %08x\n", results[i++]);
	printf("After mtic 0: %08x\n", results[i++]);
	printf("  sceKernelIsCpuIntrEnable: %08x\n", results[i++]);
	printf("  sceKernelCpuSuspendIntr: %08x\n", results[i++]);
	printf("After mtic 1: %08x\n", results[i++]);
	printf("  sceKernelIsCpuIntrEnable: %08x\n", results[i++]);
	printf("After mtic 2: %08x\n", results[i++]);
	printf("After mtic 0x80000000: %08x\n", results[i++]);
	printf("  sceKernelCpuSuspendIntr: %08x\n", results[i++]);
	printf("After sceKernelCpuResumeIntr(1): %08x\n", results[i++]);
	printf("After sceKernelCpuResumeIntr(2): %08x\n", results[i++]);
	return 0;
}
