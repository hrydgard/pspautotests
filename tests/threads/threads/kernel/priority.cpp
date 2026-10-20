#include <common.h>
#include <pspthreadman.h>

// sceKernelCreateThread from a kernel module: which priorities it takes. A user module gets
// SCE_KERNEL_ERROR_ILLEGAL_PRIORITY outside 0x08-0x77 (threads/threads/create).

static int threadFunc(SceSize argc, void *argv) {
	return 0;
}

extern "C" int main(int argc, char *argv[]) {
	static const int prios[] = { -1, 0, 1, 2, 7, 8, 0x77, 0x78, 0x7E, 0x7F, 0x80, 0xFF };

	checkpointNext("Priorities from a kernel module:");
	for (size_t i = 0; i < ARRAY_SIZE(prios); ++i) {
		SceUID thread = sceKernelCreateThread("prio", &threadFunc, prios[i], 0x1000, 0, NULL);
		if (thread > 0) {
			checkpoint("  Priority %x: OK", prios[i]);
			sceKernelDeleteThread(thread);
		} else {
			checkpoint("  Priority %x: %08x", prios[i], thread);
		}
	}
	return 0;
}
