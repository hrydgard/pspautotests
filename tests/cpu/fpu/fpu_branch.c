#include <common.h>
#include <string.h>

// The FPU condition branches: bc1f, bc1t and the likely forms bc1fl and bc1tl, taken and not,
// with the condition set by a compare and by writing fcr31 directly. The delay slot counts how
// often it ran: for the likely forms it's skipped when the branch isn't taken. There's a nop
// between the compare and the branch; fpu_branch_hazard.c is about leaving that out.

#define BRANCH(name, insn) \
	static void name(unsigned int a, unsigned int b, int *taken, int *slot) { \
		int t, s = 0; \
		asm volatile ( \
			".set noreorder\n" \
			"mtc1 %2, $f0\n" \
			"mtc1 %3, $f1\n" \
			"nop\n" \
			"c.eq.s $f0, $f1\n" \
			"nop\n" \
			insn " 1f\n" \
			"addiu %1, %1, 1\n" \
			"li %0, 0\n" \
			"j 2f\n" \
			"nop\n" \
			"1:\n" \
			"li %0, 1\n" \
			"2:\n" \
			".set reorder\n" \
			: "=r" (t), "+r" (s) : "r" (a), "r" (b) : "$f0", "$f1", "memory" \
		); \
		*taken = t; \
		*slot = s; \
	}

BRANCH(bc1f, "bc1f")
BRANCH(bc1t, "bc1t")
BRANCH(bc1fl, "bc1fl")
BRANCH(bc1tl, "bc1tl")

// The condition bit written straight into fcr31 (bit 23), with no compare.
#define BRANCH_CTC(name, insn) \
	static void name(unsigned int fcr31, int *taken, int *slot) { \
		int t, s = 0; \
		asm volatile ( \
			".set noreorder\n" \
			"ctc1 %2, $31\n" \
			"nop\n" \
			"nop\n" \
			insn " 1f\n" \
			"addiu %1, %1, 1\n" \
			"li %0, 0\n" \
			"j 2f\n" \
			"nop\n" \
			"1:\n" \
			"li %0, 1\n" \
			"2:\n" \
			".set reorder\n" \
			: "=r" (t), "+r" (s) : "r" (fcr31) : "memory" \
		); \
		*taken = t; \
		*slot = s; \
	}

BRANCH_CTC(bc1f_ctc, "bc1f")
BRANCH_CTC(bc1t_ctc, "bc1t")
BRANCH_CTC(bc1fl_ctc, "bc1fl")
BRANCH_CTC(bc1tl_ctc, "bc1tl")

// A backwards likely branch as a loop: the classic use, the delay slot being the loop body.
static int loop_bc1tl(int n) {
	int count = 0;
	asm volatile (
		".set noreorder\n"
		"mtc1 $0, $f0\n"
		"1:\n"
		"addiu %1, %1, -1\n"
		"mtc1 %1, $f1\n"
		"nop\n"
		"c.lt.s $f0, $f1\n"     // 0 < n as an int reinterpreted: true while n > 0 (positive floats)
		"nop\n"
		"bc1tl 1b\n"
		"addiu %0, %0, 1\n"
		".set reorder\n"
		: "+r" (count), "+r" (n) : : "$f0", "$f1", "memory"
	);
	return count;
}

static unsigned int readFcr31(void) {
	unsigned int v;
	asm volatile ("cfc1 %0, $31" : "=r" (v));
	return v;
}

int main(int argc, char *argv[]) {
	int taken, slot;
	static const struct { const char *name; void (*f)(unsigned int, unsigned int, int *, int *); } branches[] = {
		{ "bc1f", bc1f }, { "bc1t", bc1t }, { "bc1fl", bc1fl }, { "bc1tl", bc1tl },
	};
	static const struct { const char *name; void (*f)(unsigned int, int *, int *); } ctcBranches[] = {
		{ "bc1f", bc1f_ctc }, { "bc1t", bc1t_ctc }, { "bc1fl", bc1fl_ctc }, { "bc1tl", bc1tl_ctc },
	};

	printf("-- condition from c.eq.s --\n");
	for (int i = 0; i < ARRAY_SIZE(branches); i++) {
		branches[i].f(0x3f800000, 0x3f800000, &taken, &slot);
		printf("%-6s 1 == 1: taken=%d slot=%d fcr31=%08x\n", branches[i].name, taken, slot, readFcr31());
		branches[i].f(0x3f800000, 0x40000000, &taken, &slot);
		printf("%-6s 1 == 2: taken=%d slot=%d fcr31=%08x\n", branches[i].name, taken, slot, readFcr31());
		branches[i].f(0x7fc00000, 0x7fc00000, &taken, &slot);
		printf("%-6s nan == nan: taken=%d slot=%d fcr31=%08x\n", branches[i].name, taken, slot, readFcr31());
	}

	printf("-- condition from ctc1 --\n");
	for (int i = 0; i < ARRAY_SIZE(ctcBranches); i++) {
		ctcBranches[i].f(0x00800000, &taken, &slot);
		printf("%-6s bit 23 set: taken=%d slot=%d fcr31=%08x\n", ctcBranches[i].name, taken, slot, readFcr31());
		ctcBranches[i].f(0x00000000, &taken, &slot);
		printf("%-6s bit 23 clear: taken=%d slot=%d fcr31=%08x\n", ctcBranches[i].name, taken, slot, readFcr31());
	}

	printf("-- bc1tl loop --\n");
	printf("loop_bc1tl(5) = %d\n", loop_bc1tl(5));
	printf("loop_bc1tl(1) = %d\n", loop_bc1tl(1));
	return 0;
}
