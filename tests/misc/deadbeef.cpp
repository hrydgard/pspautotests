#include <common.h>
#include <pspthreadman.h>

static u32 regs[32];

inline void fillRegs() {
	asm volatile (
		"lui $a0, 0x1337\n"
		"lui $a1, 0x1337\n"
		"lui $a2, 0x1337\n"
		"lui $a3, 0x1337\n"
		"lui $t0, 0x1337\n"
		"lui $t1, 0x1337\n"
		"lui $t2, 0x1337\n"
		"lui $t3, 0x1337\n"
		"lui $t4, 0x1337\n"
		"lui $t5, 0x1337\n"
		"lui $t6, 0x1337\n"
		"lui $t7, 0x1337\n"
		"lui $s0, 0x1337\n"
		"lui $s1, 0x1337\n"
		"lui $s2, 0x1337\n"
		"lui $s3, 0x1337\n"
		"lui $s4, 0x1337\n"
		"lui $s5, 0x1337\n"
		"lui $s6, 0x1337\n"
		"lui $s7, 0x1337\n"
		"lui $t8, 0x1337\n"
		"lui $t9, 0x1337\n"
	);

	asm volatile (
		".set noat\n"
		"lui $at, 0x1337\n"
	);
}

inline void getRegs() {
	asm volatile (
		// First let's preserve at.
		".set noat\n"
		"addiu $v1, $at, 0\n"
		".set at\n"
		"la $v0, %0\n"
		// regs[1] -> regs[0]...
		"addiu $v0, $v0, -4\n"
		"sw $v1, 0x04($v0)\n"
		"sw $a0, 0x10($v0)\n"
		"sw $a1, 0x14($v0)\n"
		"sw $a2, 0x18($v0)\n"
		"sw $a3, 0x1C($v0)\n"
		"sw $t0, 0x20($v0)\n"
		"sw $t1, 0x24($v0)\n"
		"sw $t2, 0x28($v0)\n"
		"sw $t3, 0x2C($v0)\n"
		"sw $t4, 0x30($v0)\n"
		"sw $t5, 0x34($v0)\n"
		"sw $t6, 0x38($v0)\n"
		"sw $t7, 0x3C($v0)\n"
		"sw $s0, 0x40($v0)\n"
		"sw $s1, 0x44($v0)\n"
		"sw $s2, 0x48($v0)\n"
		"sw $s3, 0x4C($v0)\n"
		"sw $s4, 0x50($v0)\n"
		"sw $s5, 0x54($v0)\n"
		"sw $s6, 0x58($v0)\n"
		"sw $s7, 0x5C($v0)\n"
		"sw $t8, 0x60($v0)\n"
		"sw $t9, 0x64($v0)\n"
		// For some reason, regs[0] fails, regs[1] is OK.
		: "=m"(regs[1])
	);
}

inline void dumpRegs() {
	getRegs();

	checkpoint(NULL);
	schedf("at=%08x, ", regs[1]);
	schedf("a0=%08x, a1=%08x, a2=%08x, a3=%08x, ", regs[4], regs[5], regs[6], regs[7]);
	schedf("t0=%08x, t1=%08x, t2=%08x, t3=%08x, ", regs[8], regs[9], regs[10], regs[11]);
	schedf("t4=%08x, t5=%08x, t6=%08x, t7=%08x, ", regs[12], regs[13], regs[14], regs[15]);
	schedf("s0=%08x, s1=%08x, s2=%08x, s3=%08x, ", regs[16], regs[17], regs[18], regs[19]);
	schedf("s4=%08x, s5=%08x, s6=%08x, s7=%08x, ", regs[20], regs[21], regs[22], regs[23]);
	schedf("t8=%08x, t9=%08x", regs[24], regs[25]);
	schedf("\n");
}

extern "C" int main(int argc, char *argv[]) {
	checkpointNext("Reset regs:");
	fillRegs();
	dumpRegs();

	checkpointNext("Syscall:");
	fillRegs();
	sceKernelGetSystemTimeWide();
	dumpRegs();

	checkpointNext("Really long loop (interrupt):");
	fillRegs();
	for (int i = 0; i < 0x04000000; ++i) {
		continue;
	}
	dumpRegs();

	return 0;
}