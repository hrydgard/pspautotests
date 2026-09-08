#include <common.h>

#include <pspkernel.h>
#include <pspsysmem.h>

// The size passed to sceKernelCreateHeap is not a cap: a real PSP will hand out a block as large
// as the whole heap, and one twice that size, from a heap created with neither to spare. So the
// heap either extends itself out of its partition on demand or never really bounded itself.
//
// Split out of heap.c because PPSSPP doesn't do this - its heap is a fixed allocator over the
// block it reserved, and both of these come back NULL. Exactly how far the real one will grow,
// and where the memory comes from, isn't established yet; work that out before implementing it.

extern SceUID sceKernelCreateHeap(int partition, int size, int flags, const char *name);
extern int sceKernelDeleteHeap(SceUID heap);
extern void *sceKernelAllocHeapMemory(SceUID heap, int size);

#define HEAP_PARTITION 1
#define HEAP_SIZE 0x4000

int main(int argc, char *argv[]) {
	// A fresh heap for each, so one result can't depend on the one before it.
	checkpointNext("Allocating more than the heap holds:");

	SceUID heap = sceKernelCreateHeap(HEAP_PARTITION, HEAP_SIZE, 1, "heap");
	checkpoint("  Exactly the heap size: %d", sceKernelAllocHeapMemory(heap, HEAP_SIZE) != NULL);
	sceKernelDeleteHeap(heap);

	heap = sceKernelCreateHeap(HEAP_PARTITION, HEAP_SIZE, 1, "heap");
	checkpoint("  Twice the heap size: %d", sceKernelAllocHeapMemory(heap, HEAP_SIZE * 2) != NULL);
	sceKernelDeleteHeap(heap);

	// Half the heap should fit comfortably either way - a control, so a total allocation failure
	// doesn't read as "no growth".
	heap = sceKernelCreateHeap(HEAP_PARTITION, HEAP_SIZE, 1, "heap");
	checkpoint("  Half the heap size: %d", sceKernelAllocHeapMemory(heap, HEAP_SIZE / 2) != NULL);
	sceKernelDeleteHeap(heap);

	return 0;
}
