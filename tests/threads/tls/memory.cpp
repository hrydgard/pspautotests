#include "shared.h"

// The large-allocation half of the create test.
//
// These ask sceKernelCreateTlspl for a megabyte and up, so whether a given one succeeds or comes
// back with 80020190 depends on how much of the user partition is free when it runs - which is a
// property of the machine that recorded the .expected, not of the API. PSPLink itself takes a
// sizeable bite, so a recording made over the cable will report smaller limits than a game would
// see. Don't put this in tests_good, and re-record it if it starts failing for that reason
// rather than assuming the emulator broke.
//
// The parameter checking that doesn't depend on free memory lives in create.cpp.

static void testCreate(const char *title, u32 size, u32 count, SceKernelTlsplOptParam *opt) {
	u32 before = sceKernelTotalFreeMemSize();
	SceUID tls = sceKernelCreateTlspl("tls", PSP_MEMORY_PARTITION_USER, 0, size, count, opt);
	u32 after = sceKernelTotalFreeMemSize();
	checkpoint(NULL);
	schedf("%s: (allocated %d bytes) ", title, before - after);
	schedfTlspl(tls);
	if (tls > 0) {
		sceKernelDeleteTlspl(tls);
	}
	u32 freed = sceKernelTotalFreeMemSize();
	if (freed != before) {
		schedf("LEAK %d bytes\n", before - freed);
	}
}

extern "C" int main(int argc, char *argv[]) {
	char temp[128];

	checkpointNext("Free memory at start:");
	// Printed as a magnitude only - the exact figure moves with the loader and PSPLink, and
	// pinning it would make every line below look like a failure on a different setup.
	u32 freeMem = sceKernelTotalFreeMemSize();
	checkpoint("  At least 4MB free: %d", freeMem >= 4 * 1024 * 1024);
	checkpoint("  At least 8MB free: %d", freeMem >= 8 * 1024 * 1024);

	checkpointNext("Large block sizes:");
	static const u32 sizes[] = {
		0x100000, 0x1000000, 0x10000000, 0x1800000, 0x2000000,
	};
	for (size_t i = 0; i < ARRAY_SIZE(sizes); ++i) {
		sprintf(temp, "  Size 0x%08X", sizes[i]);
		testCreate(temp, sizes[i], 4, NULL);
	}

	checkpointNext("Large counts:");
	static const u32 counts[] = {
		0x1000, 0x10000, 0x100000, 0x1000000, 0x10000000, 0x1800000, 0x2000000,
	};
	for (size_t i = 0; i < ARRAY_SIZE(counts); ++i) {
		sprintf(temp, "  Count 0x%08X", counts[i]);
		testCreate(temp, 0x100, counts[i], NULL);
	}

	checkpointNext("Large alignments:");
	SceKernelTlsplOptParam opts = {0};
	static const u32 alignments[] = {
		0x100000, 0x1000000, 0x10000000, 0x1800000, 0x2000000,
	};
	for (size_t i = 0; i < ARRAY_SIZE(alignments); ++i) {
		sprintf(temp, "  Aligned to 0x%08X", alignments[i]);
		opts.size = sizeof(opts);
		opts.alignment = alignments[i];
		testCreate(temp, 1, 4, &opts);
	}

	return 0;
}
