// What the Media Engine sees at VRAM's addresses: the main CPU fills VRAM, a stub on the ME
// reads through 0x44000000 (where homebrew puts the VME's local memory) and 0xA4000000, then
// writes through both, and the main CPU checks whether the writes reached VRAM. What's there
// isn't plain RAM throughout (reading 0x44020030, mcidclan's VME "top" buffers, stalled the ME
// with the VME idle), so this stays within the first words.
#include <common.h>
#include <pspkernel.h>

#define HW(addr) (*(volatile u32 *)(addr))

extern u32 me_stub[];
extern u32 me_stub_end[];

// Runs from 0xBFC00040 after reset. Results go to 0xBFC00200: [0] progress, [2..7] what it read.
// Progress tells where it stopped if an access hangs.
asm(
	".set push\n"
	".set noreorder\n"
	".set noat\n"
	".global me_stub\n"
	".global me_stub_end\n"
	"me_stub:\n"
	"lui $8, 0xBFC0\n"
	"ori $8, $8, 0x200\n"
	"li $9, 1\n"
	"sw $9, 0($8)\n"
	"sync\n"
	// The ME's own bus clocks: DMACplus mux (0x50 bit 5, with bit 2 as homebrew sets it), VME
	// control (0x54 bit 0). Without them and the VME out of reset, writes there don't stick.
	"lui $10, 0xBC10\n"
	"lw $11, 0x50($10)\n"
	"ori $11, $11, 0x24\n"
	"sw $11, 0x50($10)\n"
	"lw $11, 0x54($10)\n"
	"ori $11, $11, 1\n"
	"sw $11, 0x54($10)\n"
	"sync\n"
	"li $9, 2\n"
	"sw $9, 0($8)\n"
	"lui $10, 0xA400\n"
	"lw $11, 0($10)\n"
	"sw $11, 8($8)\n"
	"li $9, 3\n"
	"sw $9, 0($8)\n"
	"lui $10, 0xA410\n"
	"lw $11, 0($10)\n"
	"sw $11, 12($8)\n"
	"li $9, 4\n"
	"sw $9, 0($8)\n"
	"lui $10, 0x4400\n"
	"lw $11, 0($10)\n"
	"sw $11, 16($8)\n"
	"li $9, 5\n"
	"sw $9, 0($8)\n"
	"lui $10, 0x4410\n"
	"lw $11, 0($10)\n"
	"sw $11, 20($8)\n"
	"li $9, 6\n"
	"sw $9, 0($8)\n"
	"lui $10, 0xA400\n"
	"li $11, 0x1111\n"
	"sw $11, 0x10($10)\n"
	"lui $10, 0x4400\n"
	"li $11, 0x2222\n"
	"sw $11, 0x14($10)\n"
	"sync\n"
	"li $9, 7\n"
	"sw $9, 0($8)\n"
	"lui $10, 0xA400\n"
	"lw $11, 0x10($10)\n"
	"sw $11, 24($8)\n"
	"lui $10, 0x4400\n"
	"lw $11, 0x14($10)\n"
	"sw $11, 28($8)\n"
	"li $9, 8\n"
	"sw $9, 0($8)\n"
	// Are the two windows the same memory?
	"lui $10, 0xA400\n"
	"li $11, 0x3333\n"
	"sw $11, 0x20($10)\n"
	"sync\n"
	"lui $10, 0x4400\n"
	"lw $11, 0x20($10)\n"
	"sw $11, 32($8)\n"
	"li $9, 9\n"
	"sw $9, 0($8)\n"
	"sync\n"
	"1:\n"
	"b 1b\n"
	"nop\n"
	"me_stub_end:\n"
	".set pop\n"
);

static const char *YesNo(int b) {
	return b ? "yes" : "no";
}

int main(int argc, char **argv) {
	volatile u32 *vram = (volatile u32 *)0x44000000;
	vram[0] = 0xC0DE0000;
	vram[4] = 0xC0DE0010;
	vram[5] = 0xC0DE0014;
	vram[0x100000 / 4] = 0xC0DE1000;

	// Sony's bootstrap, put back afterwards.
	u32 bootstrap[0xD4 / 4];
	for (int i = 0; i < ARRAY_SIZE(bootstrap); ++i) {
		bootstrap[i] = ((volatile u32 *)0xBFC00040)[i];
	}

	volatile u32 *result = (volatile u32 *)0xBFC00200;
	for (int i = 0; i < 16; ++i) {
		result[i] = 0;
	}
	volatile u32 *dest = (volatile u32 *)0xBFC00040;
	for (u32 *src = me_stub; src < me_stub_end; ++src) {
		*dest++ = *src;
	}
	sceKernelDcacheWritebackInvalidateAll();
	sceKernelIcacheInvalidateAll();

	// Reset the ME and the VME, and let them go.
	HW(0xBC10004C) = 0x14;
	asm volatile("sync");
	HW(0xBC10004C) = 0;
	asm volatile("sync");

	for (int i = 0; i < 100 && result[0] != 9; ++i) {
		sceKernelDelayThread(1000);
	}

	// Give the ME back to Sony's firmware, as mcidclan's MIST restarts it: the bootstrap with a0
	// outside the firmware's image makes it resume from sleep, restoring nothing (0x71C = 0) into
	// the eDRAM this left alone. Rebooting the PSP still hangs afterwards (a PSPLink reset powers it
	// off), so it needs a power cycle after this test; this at least leaves the ME running Sony's
	// code.
	volatile u32 *boot = (volatile u32 *)0xBFC00000;
	for (int i = 0; i < ARRAY_SIZE(bootstrap); ++i) {
		boot[0x40 / 4 + i] = bootstrap[i];
	}
	boot[(0x40 + 0x24) / 4] = 0x3C048840;  // lui a0, 0x8840
	boot[(0x40 + 0x28) / 4] = 0;
	boot[0x71C / 4] = 0;
	boot[0x704 / 4] = 0x1F;  // The ME's bus clocks, as MIST sets them.
	boot[0x700 / 4] = 0;
	sceKernelDcacheWritebackInvalidateAll();
	HW(0xBC10004C) |= 0x34;
	asm volatile("sync");
	HW(0xBC10004C) = 0;
	asm volatile("sync");
	int resumed = 0;
	for (int i = 0; i < 100 && !resumed; ++i) {
		resumed = boot[0x700 / 4] == 0xFFFFFFFC;
		sceKernelDelayThread(1000);
	}

	printf("ME progress: %d\n", (int)result[0]);
	printf("ME reads VRAM at 0xA4000000: %s\n", YesNo(result[2] == 0xC0DE0000));
	printf("ME reads VRAM at 0xA4100000: %s\n", YesNo(result[3] == 0xC0DE1000));
	printf("ME reads VRAM at 0x44000000: %s\n", YesNo(result[4] == 0xC0DE0000));
	printf("ME reads VRAM at 0x44100000: %s\n", YesNo(result[5] == 0xC0DE1000));
	printf("ME write through 0xA4000010 reaches VRAM: %s\n", YesNo(vram[4] == 0x1111));
	printf("ME write through 0x44000014 reaches VRAM: %s\n", YesNo(vram[5] == 0x2222));
	printf("ME reads back 0xA4000010: %s\n", YesNo(result[6] == 0x1111));
	printf("ME reads back 0x44000014: %s\n", YesNo(result[7] == 0x2222));
	printf("0xA4000020 and 0x44000020 are the same: %s\n", YesNo(result[8] == 0x3333));
	printf("Sony's ME firmware resumed: %s\n", YesNo(resumed));
	return 0;
}
