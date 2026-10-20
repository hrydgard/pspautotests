// The audio block's FIFO (0xBE000000), measured from the Media Engine, which is what feeds it: POPS's
// ME loop (popsman.prx +2F88) writes one stereo sample at a time to 0x70 while status 0x28 bit 5
// says there's room, sleeping in halt in between on the ME's interrupt source 12. The main CPU sets
// up audio as the POPS manager does (+342C) and starts a stub on the ME that sets the block up as
// the loop does, then fills the FIFO to find its depth, waits for room, plays through halt the way
// the loop does (the main CPU times it), and watches the FIFO drain. Results go to the boot page.
// Afterwards the ME goes back to Sony's firmware, but rebooting the PSP still hangs: it needs a
// power cycle after this.
#include <common.h>
#include <pspkernel.h>

int SysregC36775AD(int a, int b);
int SysregB1751B06(int index);
int Sysreg4634C9DC(int index);
int Sysreg41B0337B(int value);
int Sysreg4B4CCE80(void);
int Syscon9BC5E33B(int a);
int ClockgenDAB6E612(int hz);
int IntrDisableD774BA45(int intno);

#define HW(addr) (*(volatile u32 *)(addr))

extern u32 me_stub[];
extern u32 me_stub_end[];

// Results at 0xBFC00300 (after the stub), by word: 0 progress (phase), 1 status before filling, 2 samples until
// full, 3 status full, 4 intc raw full, 5 status with room again, 6 intc raw then, 7 spins until
// room, 8 samples played through halt (live), 9 wakeups from halt, 10 wakeups without room,
// 11 status after draining, 12 intc raw then, 14-17 the distinct statuses seen while draining,
// 18-21 the read (of 256K) each came at.
asm(
	".set push\n"
	".set noreorder\n"
	".set noat\n"
	".global me_stub\n"
	".global me_stub_end\n"
	"me_stub:\n"
	"lui $8, 0xBFC0\n"
	"ori $8, $8, 0x300\n"
	"li $9, 1\n"
	"sw $9, 0($8)\n"
	// As the manager's ME bootstrap: bus clocks, NMI flags.
	"lui $10, 0xBC10\n"
	"li $9, 0x304\n"
	"sw $9, 0x50($10)\n"
	"li $9, -1\n"
	"sw $9, 4($10)\n"
	"sync\n"
	// As the ME loop: memory protection.
	"lui $10, 0xBC00\n"
	"sw $0, 0x40($10)\n"
	"lw $11, 0x44($10)\n"
	"ins $11, $0, 0, 6\n"
	"sw $11, 0x44($10)\n"
	"lui $9, 0x4444\n"
	"ori $9, $9, 0x4444\n"
	"sw $9, 0($10)\n"
	"li $9, 0x44\n"
	"sw $9, 4($10)\n"
	"sw $0, 0xC($10)\n"
	"lw $11, 0xC($10)\n"
	"sw $11, 8($10)\n"
	// Cause 0, Status 0x20000400 (only the interrupt controller unmasked, IE clear).
	"mtc0 $0, $13\n"
	"lui $9, 0x2000\n"
	"ori $9, $9, 0x400\n"
	"mtc0 $9, $12\n"
	// The interrupt controller: source 12 only.
	"lui $25, 0xBC30\n"
	"li $9, 0x1000\n"
	"sw $9, 8($25)\n"
	"sync\n"
	// The audio block, as the loop sets it up ([0xBFC007F4] is 0 here).
	"lui $26, 0xBE00\n"
	"li $9, 1\n"
	"sw $9, 0($26)\n"
	"sw $0, 4($26)\n"
	"1:\n"
	"lw $9, 0xC($26)\n"
	"andi $9, $9, 7\n"
	"bnez $9, 1b\n"
	"nop\n"
	"sw $0, 0x50($26)\n"
	"li $9, 7\n"
	"sw $9, 0x2C($26)\n"
	"li $9, 0x22\n"
	"sw $9, 0x24($26)\n"
	"li $9, 7\n"
	"sw $9, 0x20($26)\n"
	"sw $0, 8($26)\n"
	"li $9, 0x1208\n"
	"sw $9, 0x14($26)\n"
	"sw $0, 0x18($26)\n"
	"li $9, 2\n"
	"sw $9, 0x10($26)\n"
	"li $9, 0x80\n"
	"sw $9, 0x38($26)\n"
	"li $9, 1\n"
	"sw $9, 0x40($26)\n"
	"li $9, 2\n"
	"sw $9, 4($26)\n"
	"sw $9, 0($8)\n"
	"lw $9, 0x28($26)\n"
	"sw $9, 4($8)\n"
	// 1: fill it.
	"li $11, 0\n"
	"li $12, 4096\n"
	"2:\n"
	"lw $9, 0x28($26)\n"
	"andi $9, $9, 0x20\n"
	"beqz $9, 3f\n"
	"nop\n"
	"sw $0, 0x70($26)\n"
	"addiu $11, $11, 1\n"
	"bne $11, $12, 2b\n"
	"nop\n"
	"3:\n"
	"sw $11, 8($8)\n"
	"lw $9, 0x28($26)\n"
	"sw $9, 12($8)\n"
	"lw $9, 0($25)\n"
	"sw $9, 16($8)\n"
	"li $9, 3\n"
	"sw $9, 0($8)\n"
	// 2: wait for room.
	"li $11, 0\n"
	"lui $12, 0x100\n"
	"4:\n"
	"lw $9, 0x28($26)\n"
	"andi $10, $9, 0x20\n"
	"bnez $10, 5f\n"
	"nop\n"
	"addiu $11, $11, 1\n"
	"bne $11, $12, 4b\n"
	"nop\n"
	"5:\n"
	"sw $9, 20($8)\n"
	"lw $9, 0($25)\n"
	"sw $9, 24($8)\n"
	"sw $11, 28($8)\n"
	"li $9, 4\n"
	"sw $9, 0($8)\n"
	// 3: play 5000 samples, sleeping in halt as the loop does.
	"li $11, 0\n"
	"li $13, 0\n"
	"li $14, 0\n"
	"li $12, 5000\n"
	"6:\n"
	".word 0x70000000\n"
	"addiu $13, $13, 1\n"
	"lw $9, 0x28($26)\n"
	"andi $10, $9, 0x20\n"
	"bnez $10, 7f\n"
	"nop\n"
	"addiu $14, $14, 1\n"
	"b 8f\n"
	"nop\n"
	"7:\n"
	"sw $0, 0x70($26)\n"
	"addiu $11, $11, 1\n"
	"sw $11, 32($8)\n"
	"8:\n"
	"sw $13, 36($8)\n"
	"sw $14, 40($8)\n"
	"bne $11, $12, 6b\n"
	"nop\n"
	"li $9, 5\n"
	"sw $9, 0($8)\n"
	// 4: stop writing and watch it drain: the distinct statuses in order, and the read each came at.
	"li $11, 0\n"
	"lui $12, 0x4\n"
	"li $15, -1\n"
	"addiu $24, $8, 56\n"
	"addiu $16, $8, 72\n"
	"9:\n"
	"lw $9, 0x28($26)\n"
	"beq $9, $15, 10f\n"
	"nop\n"
	"move $15, $9\n"
	"beq $24, $16, 10f\n"
	"nop\n"
	"sw $9, 0($24)\n"
	"sw $11, 16($24)\n"
	"addiu $24, $24, 4\n"
	"10:\n"
	"addiu $11, $11, 1\n"
	"bne $11, $12, 9b\n"
	"nop\n"
	"lw $9, 0x28($26)\n"
	"sw $9, 44($8)\n"
	"lw $9, 0($25)\n"
	"sw $9, 48($8)\n"
	"li $9, 6\n"
	"sw $9, 0($8)\n"
	"sync\n"
	"11:\n"
	"b 11b\n"
	"nop\n"
	"me_stub_end:\n"
	".set pop\n"
);

static u32 Now() {
	return sceKernelGetSystemTimeLow();
}

// Waits up to timeoutUs for the word to reach at least value. Returns whether it did.
static int WaitFor(volatile u32 *word, u32 value, u32 timeoutUs) {
	const u32 start = Now();
	while (*word < value) {
		if (Now() - start > timeoutUs) {
			return 0;
		}
	}
	return 1;
}

int main(int argc, char **argv) {
	volatile u32 *boot = (volatile u32 *)0xBFC00000;
	volatile u32 *result = (volatile u32 *)0xBFC00300;
	u32 bootstrap[0xD4 / 4];
	for (int i = 0; i < (int)ARRAY_SIZE(bootstrap); ++i) {
		bootstrap[i] = boot[0x40 / 4 + i];
	}
	for (int i = 0; i < 24; ++i) {
		result[i] = 0;
	}

	// The manager's audio setup (+342C).
	SysregC36775AD(0, 1);
	SysregB1751B06(0);
	Sysreg4634C9DC(0);
	Sysreg41B0337B(0);
	Sysreg4B4CCE80();
	HW(0xBC10006C) = 1;
	Syscon9BC5E33B(0);
	ClockgenDAB6E612(44100);

	// The audio interrupt (10), which audio.prx handles here but isn't loaded in a POPS boot: with the
	// ME feeding the block, it left the main CPU stuck.
	IntrDisableD774BA45(10);

	// Releasing the ME as the manager does (+35D8), with the stub at 0xBFC00040.
	HW(0xBC10004C) |= 0x24;
	HW(0xBC100050) |= 1;
	HW(0xBC100070) &= ~4;
	for (u32 *src = me_stub; src < me_stub_end; ++src) {
		boot[0x40 / 4 + (src - me_stub)] = *src;
	}
	sceKernelDcacheWritebackInvalidateAll();
	HW(0xBC10004C) &= ~4;

	WaitFor(&result[0], 4, 1000000);
	int timed = 0;
	u32 elapsed = 0;
	if (WaitFor(&result[8], 100, 1000000)) {
		const u32 start = Now();
		if (WaitFor(&result[8], 4500, 1000000)) {
			elapsed = Now() - start;
			timed = 1;
		}
	}
	// The drain's 256K reads, timed to turn read counts into time.
	WaitFor(&result[0], 5, 1000000);
	const u32 drainStart = Now();
	const int drained = WaitFor(&result[0], 6, 1000000);
	const u32 drainTime = Now() - drainStart;
	u32 r[24];
	for (int i = 0; i < 24; ++i) {
		r[i] = result[i];
	}

	// Back to Sony's firmware, as in me/vram.
	for (int i = 0; i < (int)ARRAY_SIZE(bootstrap); ++i) {
		boot[0x40 / 4 + i] = bootstrap[i];
	}
	boot[(0x40 + 0x24) / 4] = 0x3C048840;  // lui a0, 0x8840
	boot[(0x40 + 0x28) / 4] = 0;
	boot[0x71C / 4] = 0;
	boot[0x704 / 4] = 0x1F;
	boot[0x700 / 4] = 0;
	sceKernelDcacheWritebackInvalidateAll();
	HW(0xBC10004C) |= 0x34;
	asm volatile("sync");
	HW(0xBC10004C) = 0;
	asm volatile("sync");

	printf("Reached phase: %d\n", (int)r[0]);
	printf("Status before filling: %08x\n", (unsigned)r[1]);
	printf("Samples until full: %d\n", (int)r[2]);
	printf("Status full: %08x, interrupts raised %08x\n", (unsigned)r[3], (unsigned)r[4]);
	printf("Status with room: %08x, interrupts raised %08x\n", (unsigned)r[5], (unsigned)r[6]);
	printf("Played through halt: %d, woke %d times, %d without room\n", (int)r[8], (int)r[9], (int)r[10]);
	if (timed) {
		// 4400 samples, as a rate rounded to 100Hz.
		printf("Rate: about %d Hz\n", (int)((4400ULL * 1000000 / elapsed + 50) / 100 * 100));
	} else {
		printf("Rate: not measured\n");
	}
	// The distinct statuses while draining, and when each came (rounded to 100us). An unused slot
	// is 0 and came at read 0.
	printf("Statuses while draining:\n");
	for (int i = 14; i < 18; ++i) {
		if (i > 14 && r[i] == 0 && r[i + 4] == 0) {
			break;
		}
		const u32 us = drained ? (u32)((unsigned long long)r[i + 4] * drainTime / 0x40000) : 0;
		printf("  %08x at about %dus\n", (unsigned)r[i], (int)((us + 50) / 100 * 100));
	}
	printf("Status drained: %08x, interrupts raised %08x\n", (unsigned)r[11], (unsigned)r[12]);
	return 0;
}
