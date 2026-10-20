#include <common.h>

// sceKernelRegisterIntrHandler and its kernel-only siblings: what they return. Interrupts are 0-67.
// 33 isn't used by anything, so the handler registered there never runs. The ones the firmware's own
// drivers use (67 among them) answer 80020067 to a second handler, and aren't touched here, since
// releasing one takes the driver's handler away.

extern "C" {
int sceKernelRegisterIntrHandler(int intno, int no, void *handler, void *arg1, void *arg2);
int sceKernelReleaseIntrHandler(int intno);
int sceKernelEnableIntr(int intno);
int sceKernelDisableIntr(int intno);
}

static int handler(void *arg) {
	return -1;
}

static const int FREE_INTR = 33;

extern "C" int main(int argc, char *argv[]) {
	static const int invalid[] = { -1, 68, 100 };
	checkpointNext("Invalid numbers:");
	for (size_t i = 0; i < ARRAY_SIZE(invalid); ++i) {
		const int n = invalid[i];
		checkpoint("  %d: register %08x release %08x enable %08x disable %08x", n,
			sceKernelRegisterIntrHandler(n, 0, (void *)&handler, NULL, NULL),
			sceKernelReleaseIntrHandler(n), sceKernelEnableIntr(n), sceKernelDisableIntr(n));
	}

	checkpointNext("No handler yet:");
	checkpoint("  Release: %08x", sceKernelReleaseIntrHandler(FREE_INTR));
	checkpoint("  Enable: %08x", sceKernelEnableIntr(FREE_INTR));
	checkpoint("  Disable: %08x", sceKernelDisableIntr(FREE_INTR));

	checkpointNext("Registering:");
	int result = sceKernelRegisterIntrHandler(FREE_INTR, 0, NULL, NULL, NULL);
	checkpoint("  NULL handler: %08x", result);
	if (result == 0)
		sceKernelReleaseIntrHandler(FREE_INTR);
	checkpoint("  Handler: %08x", sceKernelRegisterIntrHandler(FREE_INTR, 0, (void *)&handler, NULL, NULL));
	checkpoint("  Again: %08x", sceKernelRegisterIntrHandler(FREE_INTR, 0, (void *)&handler, NULL, NULL));

	checkpointNext("Enabling:");
	checkpoint("  Enable: %08x", sceKernelEnableIntr(FREE_INTR));
	checkpoint("  Enable again: %08x", sceKernelEnableIntr(FREE_INTR));
	checkpoint("  Disable: %08x", sceKernelDisableIntr(FREE_INTR));
	checkpoint("  Disable again: %08x", sceKernelDisableIntr(FREE_INTR));

	checkpointNext("Releasing:");
	checkpoint("  Release: %08x", sceKernelReleaseIntrHandler(FREE_INTR));
	checkpoint("  Release again: %08x", sceKernelReleaseIntrHandler(FREE_INTR));
	return 0;
}
