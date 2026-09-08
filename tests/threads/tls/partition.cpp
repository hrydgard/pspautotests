#include "shared.h"

extern "C" {
#include <sysmem-imports.h>
}

// Which memory partitions will sceKernelCreateTlspl accept, and does the answer move with the
// compiled SDK version the caller declares?
//
// create.cpp records the partition results for one setting only. The kernel gates plenty of
// other behaviour on sceKernelSetCompiledSdkVersion, so it's worth knowing whether this is one
// of them before an emulator hard-codes a single range. The firmware version is printed too, so
// a recording made on different firmware is self-identifying rather than just "different".
//
// This runs in user mode, like everything else here. Some partitions are documented as
// kernel-only, so a rejection below means "not from user mode" rather than "no such partition" -
// a kernel-mode recording would need a much smaller test module than this suite's common code
// builds, since the kernel partition only has a couple of hundred KB free with PSPLink loaded.

static void testPartitions(const char *title) {
	char temp[128];
	static const int parts[] = { -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
	checkpointNext(title);
	for (size_t i = 0; i < ARRAY_SIZE(parts); ++i) {
		SceUID tls = sceKernelCreateTlspl("tls", parts[i], 0, 0x100, 4, NULL);
		// The UID itself depends on what else has been allocated, so only report whether it
		// worked - the error code is the interesting part.
		if (tls > 0) {
			sprintf(temp, "  Partition %d: OK", parts[i]);
		} else {
			sprintf(temp, "  Partition %d: %08x", parts[i], tls);
		}
		checkpoint("%s", temp);
		if (tls > 0) {
			sceKernelDeleteTlspl(tls);
		}
	}
}

extern "C" int main(int argc, char *argv[]) {
	checkpointNext("Firmware:");
	// Major and minor only - the point release moves between units and emulator settings, and
	// pinning it would make this look like a failure everywhere but the machine it was recorded on.
	int devkit = sceKernelDevkitVersion();
	checkpoint("  firmware %d.%02d", (devkit >> 24) & 0xFF, (devkit >> 16) & 0xFF);

	testPartitions("As launched:");

	sceKernelSetCompiledSdkVersion(0x01000010);
	testPartitions("Compiled SDK 1.00:");

	sceKernelSetCompiledSdkVersion(0x03000010);
	testPartitions("Compiled SDK 3.00:");

	sceKernelSetCompiledSdkVersion395(0x03090510);
	testPartitions("Compiled SDK 3.95:");

	sceKernelSetCompiledSdkVersion500_505(0x05000010);
	testPartitions("Compiled SDK 5.00:");

	sceKernelSetCompiledSdkVersion606(0x06060010);
	testPartitions("Compiled SDK 6.06:");

	return 0;
}
