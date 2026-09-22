#include <common.h>

// What div and divu leave in LO and HI when the divisor is zero, and on the one signed
// overflow (INT_MIN / -1). Neither has a defined result in the MIPS architecture, so the
// answer is whatever the Allegrex does, and every JIT backend has its own sequence for it.
//
// NOTE: The two-operand "div rs, rt" is an assembler macro that checks for exactly these two
// cases and emits a break, which hangs the test. The three-operand form is the raw instruction.

__attribute__((noinline)) static void do_div(int num, int denom, unsigned int *lo, unsigned int *hi) {
	asm volatile (
		".set noat\n"
		"div $0, %2, %3\n"
		".set at\n"
		"nop\n"
		"mflo %0\n"
		"mfhi %1\n"
		: "=r"(*lo), "=r"(*hi)
		: "r"(num), "r"(denom)
	);
}

__attribute__((noinline)) static void do_divu(unsigned int num, unsigned int denom, unsigned int *lo, unsigned int *hi) {
	asm volatile (
		".set noat\n"
		"divu $0, %2, %3\n"
		".set at\n"
		"nop\n"
		"mflo %0\n"
		"mfhi %1\n"
		: "=r"(*lo), "=r"(*hi)
		: "r"(num), "r"(denom)
	);
}

static const int numerators[] = {
	0, 1, -1, 2, 100, -100, 0x7FFFFFFF, (int)0x80000000, 0x12345678, (int)0xF2345678,
};

int main(int argc, char *argv[]) {
	unsigned int lo, hi;

	printf("-- div by zero --\n");
	for (int i = 0; i < ARRAY_SIZE(numerators); i++) {
		do_div(numerators[i], 0, &lo, &hi);
		printf("div %08x / 0: lo=%08x hi=%08x\n", numerators[i], lo, hi);
	}

	printf("-- divu by zero --\n");
	for (int i = 0; i < ARRAY_SIZE(numerators); i++) {
		do_divu(numerators[i], 0, &lo, &hi);
		printf("divu %08x / 0: lo=%08x hi=%08x\n", numerators[i], lo, hi);
	}

	printf("-- overflow --\n");
	do_div((int)0x80000000, -1, &lo, &hi);
	printf("div 80000000 / -1: lo=%08x hi=%08x\n", lo, hi);
	do_divu(0x80000000, 0xFFFFFFFF, &lo, &hi);
	printf("divu 80000000 / ffffffff: lo=%08x hi=%08x\n", lo, hi);

	printf("-- normal --\n");
	for (int i = 0; i < ARRAY_SIZE(numerators); i++) {
		do_div(numerators[i], 7, &lo, &hi);
		printf("div %08x / 7: lo=%08x hi=%08x\n", numerators[i], lo, hi);
		do_div(numerators[i], -7, &lo, &hi);
		printf("div %08x / -7: lo=%08x hi=%08x\n", numerators[i], lo, hi);
		do_divu(numerators[i], 7, &lo, &hi);
		printf("divu %08x / 7: lo=%08x hi=%08x\n", numerators[i], lo, hi);
	}
	return 0;
}
