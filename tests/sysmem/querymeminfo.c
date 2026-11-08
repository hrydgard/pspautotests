// sceKernelQueryMemoryInfo(address, &partition, &block): which partition and memory block an
// address is in.

#include <common.h>

#include <pspkernel.h>
#include <pspsysmem.h>
#include <pspthreadman.h>
#include <malloc.h>

int sceKernelQueryMemoryInfo(u32 address, int *partition, SceUID *block);

static SceUID allocated;
static int globalVar;

static void describeBlock(u32 address, SceUID block, int showOffset) {
	if (block == allocated) {
		printf(", the allocated block");
		return;
	}
	void *head = sceKernelGetBlockHeadAddr(block);
	if ((int)head < 0 || head == NULL) {
		printf(", block %08x (head %08x)", block, (unsigned int)(u32)head);
	} else if (showOffset) {
		printf(", another block, address at head+%x", (unsigned int)(address - (u32)head));
	} else {
		printf(", another block, address %s head", address >= (u32)head ? "after" : "before");
	}
}

static void testQueryOffset(const char *title, u32 address, int showOffset) {
	int partition = 0x12345678;
	SceUID block = 0x12345678;
	int result = sceKernelQueryMemoryInfo(address, &partition, &block);
	printf("%s: %08x, partition=%08x", title, result, partition);
	if (block == 0x12345678) {
		printf(", block untouched");
	} else {
		describeBlock(address, block, showOffset);
	}
	printf("\n");
}

static void testQuery(const char *title, u32 address) {
	testQueryOffset(title, address, 1);
}

static void testPartition(const char *title, u32 address) {
	int partition = 0x12345678;
	int result = sceKernelQueryMemoryInfo(address, &partition, NULL);
	printf("%s, partition only: %08x, partition=%08x\n", title, result, partition);
}

int main(int argc, char *argv[]) {
	int stackVar = 0;

	allocated = sceKernelAllocPartitionMemory(PSP_MEMORY_PARTITION_USER, "querymeminfo", PSP_SMEM_Low, 0x1000, NULL);
	u32 head = (u32)sceKernelGetBlockHeadAddr(allocated);
	printf("Allocated: %s\n", allocated > 0 ? "yes" : "no");

	testQuery("Block head", head);
	testQuery("Block last byte", head + 0xFFF);
	testQuery("Block end", head + 0x1000);
	testQuery("Stack", (u32)&stackVar);
	testQuery("Global", (u32)&globalVar);
	testQuery("Code", (u32)&main);
	void *heap = malloc(16);
	// Where in the heap depends on what was allocated before.
	testQueryOffset("malloc", (u32)heap, 0);
	free(heap);
	testQuery("Past RAM", 0x0C000000);
	testQuery("Kernel partition (uncached)", 0x08000000);
	testQuery("VRAM", 0x04000000);
	testQuery("Scratchpad", 0x00010000);
	testQuery("Address 0", 0);
	testQuery("Kernel address", 0x88800000);
	testQuery("Uncached block head", head | 0x40000000);

	int partition = 0x12345678;
	SceUID block = 0x12345678;
	int result = sceKernelQueryMemoryInfo(head, NULL, &block);
	printf("NULL partition pointer: %08x, block %s\n", result, block == allocated ? "the allocated block" : "other");
	result = sceKernelQueryMemoryInfo(head, &partition, NULL);
	printf("NULL block pointer: %08x, partition=%08x\n", result, partition);
	result = sceKernelQueryMemoryInfo(head, NULL, NULL);
	printf("Both NULL: %08x\n", result);
	result = sceKernelQueryMemoryInfo(head, (int *)0x88000000, &block);
	printf("Kernel partition pointer: %08x\n", result);
	result = sceKernelQueryMemoryInfo(head, &partition, (SceUID *)0x88000000);
	printf("Kernel block pointer: %08x\n", result);

	// With no block pointer, only the partition is looked up. What else is in user memory depends
	// on PSPLink, so the partition edges are only tested this way. Where the user partition ends
	// depends on the RAM size the program got (PSPLink: 0x0A000000), so that's left out.
	testPartition("User partition start", 0x08800000);
	testPartition("User partition last byte", 0x09FFFFFF);
	testPartition("Past RAM", 0x0C000000);
	testPartition("Kernel partition start", 0x08000000);
	testPartition("0x082FFFFF", 0x082FFFFF);
	testPartition("Kernel partition 0x08300000", 0x08300000);
	testPartition("Volatile memory 0x08400000", 0x08400000);
	testPartition("0x087FFFFF", 0x087FFFFF);
	testPartition("Uncached user memory", 0x48800000);
	testPartition("Mirror 0x28800000", 0x28800000);
	testPartition("VRAM", 0x04000000);
	testPartition("Global", (u32)&globalVar);
	testPartition("Block end", head + 0x1000);

	sceKernelFreePartitionMemory(allocated);
	testQuery("Freed block head", head);
	testPartition("Freed block head", head);

	SceUID high = sceKernelAllocPartitionMemory(PSP_MEMORY_PARTITION_USER, "querymeminfo", PSP_SMEM_High, 0x100, NULL);
	u32 highHead = (u32)sceKernelGetBlockHeadAddr(high);
	allocated = high;
	testQuery("High block head", highHead);
	sceKernelFreePartitionMemory(high);

	return 0;
}
