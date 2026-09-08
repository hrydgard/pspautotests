// Shared by tests/sysmem/partitions (user mode) and tests/sysmem/kernel/partitions (kernel mode).
// The two builds run exactly the same sweep, so diffing their .expected files shows precisely what
// the caller's privilege buys - which rejections mean "no such partition" and which mean "not from
// where you're asking".
//
// These APIs all look the partition up the same way, so it would be easy to assume they agree.
// They don't: sceKernelAllocPartitionMemory rejects the kernel partitions with ILLEGAL_PARTITION
// where the ThreadMan ones say ILLEGAL_PERM, and threads/tls/create and threads/vpl/create already
// disagree with each other about partitions 8 and 9.

#include <common.h>

#include <pspkernel.h>
#include <pspsysmem.h>
#include <pspthreadman.h>

static const int sweepPartitions[] = { -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

static void formatResult(char *out, int result) {
	if (result > 0) {
		strcpy(out, "OK");
	} else {
		sprintf(out, "%08x", result);
	}
}

static void sweepPartition(int part) {
	char allocStr[16], vplStr[16], fplStr[16], pipeStr[16];

	// Only the success or the error code is reproducible - the UID itself depends on what else
	// happens to be allocated.
	SceUID alloc = sceKernelAllocPartitionMemory(part, "sweep", PSP_SMEM_Low, 0x100, NULL);
	formatResult(allocStr, alloc);
	if (alloc > 0) {
		sceKernelFreePartitionMemory(alloc);
	}

	// sceKernelCreateVpl on the volatile partition never returns - it blocks forever rather than
	// refusing, presumably waiting on the volatile memory lock, and takes the whole test with it.
	// The other three are fine there, so this skip is as narrow as it can be. Don't "fix" it by
	// putting the call back.
	if (part == 5) {
		strcpy(vplStr, "BLOCKS");
	} else {
		SceUID vpl = sceKernelCreateVpl("sweep", part, 0, 0x1000, NULL);
		formatResult(vplStr, vpl);
		if (vpl > 0) {
			sceKernelDeleteVpl(vpl);
		}
	}

	SceUID fpl = sceKernelCreateFpl("sweep", part, 0, 0x100, 4, NULL);
	formatResult(fplStr, fpl);
	if (fpl > 0) {
		sceKernelDeleteFpl(fpl);
	}

	SceUID pipe = sceKernelCreateMsgPipe("sweep", part, 0, (void *)0x1000, NULL);
	formatResult(pipeStr, pipe);
	if (pipe > 0) {
		sceKernelDeleteMsgPipe(pipe);
	}

	printf("  Partition %d: alloc=%s vpl=%s fpl=%s msgpipe=%s\n",
		part, allocStr, vplStr, fplStr, pipeStr);
}

static void sweepAllPartitions(const char *title) {
	printf("%s\n", title);
	for (size_t i = 0; i < ARRAY_SIZE(sweepPartitions); ++i) {
		sweepPartition(sweepPartitions[i]);
	}
}
