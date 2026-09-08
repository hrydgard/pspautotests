#include <common.h>

#include <pspkernel.h>
#include <pspsysmem.h>

// sceKernelCreateHeap and friends, which nothing has ever tested - they're only reachable from
// kernel code, which is why. Every error code an emulator returns here is currently a guess.
//
// This file sticks to operations that leave the kernel in a good state. Two categories don't:
// creating a heap in the volatile partition (5) kills the console outright, and the calls that
// abuse the allocator - double frees, foreign pointers, deleting a heap with a block still out -
// are untested here. Add those one at a time, with unbuffered output, and expect to reboot.

extern SceUID sceKernelCreateHeap(int partition, int size, int flags, const char *name);
extern int sceKernelDeleteHeap(SceUID heap);
extern void *sceKernelAllocHeapMemory(SceUID heap, int size);
extern int sceKernelFreeHeapMemory(SceUID heap, void *block);
extern void *sceKernelAllocHeapMemoryWithOption(SceUID heap, int size, void *option);
extern int sceKernelPartitionTotalFreeMemSize(int partition);

#define HEAP_PARTITION 1
#define HEAP_SIZE 0x4000

static void testCreate(const char *title, int partition, int size, int flags, const char *name) {
	// Creating a heap in the volatile partition takes the kernel down - not an error return, the
	// console stops answering and needs a reboot. Same partition that makes sceKernelCreateVpl
	// block forever in tests/sysmem/partitions, so the volatile partition evidently wants
	// sceKernelVolatileMemLock before anyone carves anything out of it. Don't put this back.
	if (partition == 5) {
		checkpoint("%s: CRASHES", title);
		return;
	}
	SceUID heap = sceKernelCreateHeap(partition, size, flags, name);
	if (heap > 0) {
		checkpoint("%s: OK", title);
		sceKernelDeleteHeap(heap);
	} else {
		checkpoint("%s: %08x", title, heap);
	}
}

int main(int argc, char *argv[]) {
	char temp[128];
	int i;

	checkpointNext("Partitions:");
	static const int parts[] = { -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
	for (i = 0; i < (int)ARRAY_SIZE(parts); ++i) {
		sprintf(temp, "  Partition %d", parts[i]);
		testCreate(temp, parts[i], HEAP_SIZE, 1, "heap");
	}

	checkpointNext("Sizes:");
	// Nothing here may depend on how much of the kernel partition happens to be free: it is only
	// a few hundred KB with PSPLink resident, so a megabyte fails for reasons that aren't the API.
	static const int sizes[] = { -1, 0, 1, 0x10, 0x40, 0x100, 0x1000 };
	for (i = 0; i < (int)ARRAY_SIZE(sizes); ++i) {
		sprintf(temp, "  Size 0x%08X", sizes[i]);
		testCreate(temp, HEAP_PARTITION, sizes[i], 1, "heap");
	}

	checkpointNext("Flags:");
	static const int flags[] = { -1, 0, 1, 2, 3, 4, 0x100, 0x1000 };
	for (i = 0; i < (int)ARRAY_SIZE(flags); ++i) {
		sprintf(temp, "  Flags %d", flags[i]);
		testCreate(temp, HEAP_PARTITION, HEAP_SIZE, flags[i], "heap");
	}

	checkpointNext("Names:");
	testCreate("  Normal name", HEAP_PARTITION, HEAP_SIZE, 1, "heap");
	testCreate("  Empty name", HEAP_PARTITION, HEAP_SIZE, 1, "");
	testCreate("  NULL name", HEAP_PARTITION, HEAP_SIZE, 1, NULL);

	checkpointNext("Does it come out of the partition it asked for:");
	{
		int before = sceKernelPartitionTotalFreeMemSize(HEAP_PARTITION);
		SceUID heap = sceKernelCreateHeap(HEAP_PARTITION, HEAP_SIZE, 1, "heap");
		int during = sceKernelPartitionTotalFreeMemSize(HEAP_PARTITION);
		if (heap > 0) {
			sceKernelDeleteHeap(heap);
		}
		int after = sceKernelPartitionTotalFreeMemSize(HEAP_PARTITION);
		// Exact figures move with whatever else is loaded, so only the shape is reproducible.
		checkpoint("  Took at least the heap size: %d", before - during >= HEAP_SIZE);
		checkpoint("  Gave it all back on delete: %d", after == before);
	}

	checkpointNext("Allocating:");
	{
		SceUID heap = sceKernelCreateHeap(HEAP_PARTITION, HEAP_SIZE, 1, "heap");
		if (heap <= 0) {
			checkpoint("  Could not create a heap to allocate from: %08x", heap);
		} else {
			void *a = sceKernelAllocHeapMemory(heap, 0x100);
			checkpoint("  0x100: %d", a != NULL);
			void *b = sceKernelAllocHeapMemory(heap, 0x100);
			checkpoint("  Second 0x100 is a different block: %d", b != NULL && b != a);
			checkpoint("  Zero bytes: %d", sceKernelAllocHeapMemory(heap, 0) != NULL);
			checkpoint("  Free a block: %08x", sceKernelFreeHeapMemory(heap, a));
			checkpoint("  Free the other block: %08x", sceKernelFreeHeapMemory(heap, b));
			// Now that both are back, the space should be reusable.
			void *c = sceKernelAllocHeapMemory(heap, 0x100);
			checkpoint("  Allocate again after freeing: %d", c != NULL);
			if (c != NULL) {
				sceKernelFreeHeapMemory(heap, c);
			}
			checkpoint("  Delete: %08x", sceKernelDeleteHeap(heap));
		}
	}

	// A fresh heap each time, so whether one of these succeeds can't depend on the ones before it.
	checkpointNext("Allocating more than the heap holds:");
	{
		SceUID heap = sceKernelCreateHeap(HEAP_PARTITION, HEAP_SIZE, 1, "heap");
		checkpoint("  Exactly the heap size: %d", sceKernelAllocHeapMemory(heap, HEAP_SIZE) != NULL);
		sceKernelDeleteHeap(heap);

		heap = sceKernelCreateHeap(HEAP_PARTITION, HEAP_SIZE, 1, "heap");
		// Whether this fails or quietly extends the heap is the question.
		checkpoint("  Twice the heap size: %d", sceKernelAllocHeapMemory(heap, HEAP_SIZE * 2) != NULL);
		sceKernelDeleteHeap(heap);
	}

	checkpointNext("AllocHeapMemoryWithOption:");
	{
		SceUID heap = sceKernelCreateHeap(HEAP_PARTITION, HEAP_SIZE, 1, "heap");
		if (heap <= 0) {
			checkpoint("  Could not create a heap: %08x", heap);
		} else {
			// The option block is a size followed by an alignment, like the other kernel option
			// structs. Anything else should be refused.
			unsigned int option[2];
			static const unsigned int optionSizes[] = { 0, 4, 8, 12, 0x100 };
			for (i = 0; i < (int)ARRAY_SIZE(optionSizes); ++i) {
				option[0] = optionSizes[i];
				// 4 is known to be an accepted alignment, so this really does vary only the size.
				option[1] = 4;
				sprintf(temp, "  Option size %d", optionSizes[i]);
				checkpoint("%s: %d", temp, sceKernelAllocHeapMemoryWithOption(heap, 0x100, option) != NULL);
			}
			checkpoint("  NULL option: %d", sceKernelAllocHeapMemoryWithOption(heap, 0x100, NULL) != NULL);
			sceKernelDeleteHeap(heap);
		}
	}

	checkpointNext("Alignment:");
	{
		SceUID heap = sceKernelCreateHeap(HEAP_PARTITION, HEAP_SIZE, 1, "heap");
		if (heap <= 0) {
			checkpoint("  Could not create a heap: %08x", heap);
		} else {
			unsigned int option[2];
			// Densely enough spaced to pin where the accepted range stops, which is what an
			// implementation actually needs to know.
			static const unsigned int alignments[] = {
				0, 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x1000,
			};
			for (i = 0; i < (int)ARRAY_SIZE(alignments); ++i) {
				option[0] = 8;
				option[1] = alignments[i];
				void *p = sceKernelAllocHeapMemoryWithOption(heap, 0x40, option);
				sprintf(temp, "  Aligned to 0x%X", alignments[i]);
				if (p == NULL) {
					checkpoint("%s: failed", temp);
				} else {
					// Report whether the alignment was honoured, not the address, which moves.
					unsigned int mask = alignments[i] ? alignments[i] - 1 : 0;
					checkpoint("%s: aligned=%d", temp, (((unsigned int)p) & mask) == 0);
				}
			}
			sceKernelDeleteHeap(heap);
		}
	}

	return 0;
}
