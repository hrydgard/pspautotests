#include <common.h>
#include <pspkernel.h>

#include <stdint.h>

int return_1() {
	return 1;
}
void return_1_end() {}

int return_2() {
	return 2;
}
void return_2_end() {}

int return_3() {
	return 3;
}
void return_3_end() {}

typedef int (*func_ptr)();

u8 *MakePointerUncached(volatile u8 *ptr) {
	return (u8 *)((uintptr_t)ptr | 0x40000000);
}

void try_run_code_at(volatile u8 *addr, func_ptr start, void (*end)()) {
	u8 *uncached_addr = MakePointerUncached(addr);
	memcpy(uncached_addr, start, (u8 *)end - (u8 *)start);
	sceKernelIcacheInvalidateRange((void *)addr, (u32)((u8 *)end - (u8 *)start));
	volatile func_ptr func = (func_ptr)addr;
	int retval = func();
	schedf("Copied to %p, ran code at %p, returned %d\n", uncached_addr, addr, retval);
}

int main(int argc, char *argv[]) {
	// Somewhere in the middle of RAM.
	volatile u8 *ram_addr = (u8 *)0x09000000;
	volatile u8 *scratch_addr = (u8 *)0x00010000;
	volatile u8 *vram_addr = (u8 *)0x04000000;

	try_run_code_at(ram_addr, return_1, return_1_end);
	try_run_code_at(vram_addr, return_2, return_2_end);
	try_run_code_at(scratch_addr, return_3, return_3_end);
	return 0;
}