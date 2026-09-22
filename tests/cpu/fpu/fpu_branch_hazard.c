#include <common.h>
#include <string.h>

// FPU condition hazards: a compare (or ctc1) with the branch or cfc1 right behind it, no nop.
// Classic MIPS needs one instruction between them; the compiler pads for this, so an emulator
// doesn't have to match, but it's good to know what the hardware does.

// The condition is first set to the opposite of what the compare gives, so a stale read shows.
#define HAZARD(name, insn) \
	static void name(unsigned int a, unsigned int b, unsigned int oldFcr31, int *taken, int *slot) { \
		int t, s = 0; \
		asm volatile ( \
			".set noreorder\n" \
			"mtc1 %2, $f0\n" \
			"mtc1 %3, $f1\n" \
			"ctc1 %4, $31\n" \
			"nop\n" \
			"nop\n" \
			"c.eq.s $f0, $f1\n" \
			insn " 1f\n" \
			"addiu %1, %1, 1\n" \
			"li %0, 0\n" \
			"j 2f\n" \
			"nop\n" \
			"1:\n" \
			"li %0, 1\n" \
			"2:\n" \
			".set reorder\n" \
			: "=r" (t), "+r" (s) : "r" (a), "r" (b), "r" (oldFcr31) : "$f0", "$f1", "memory" \
		); \
		*taken = t; \
		*slot = s; \
	}

HAZARD(bc1f_h, "bc1f")
HAZARD(bc1t_h, "bc1t")
HAZARD(bc1fl_h, "bc1fl")
HAZARD(bc1tl_h, "bc1tl")

// ctc1 immediately followed by the branch.
#define HAZARD_CTC(name, insn) \
	static void name(unsigned int oldFcr31, unsigned int fcr31, int *taken, int *slot) { \
		int t, s = 0; \
		asm volatile ( \
			".set noreorder\n" \
			"ctc1 %2, $31\n" \
			"nop\n" \
			"nop\n" \
			"ctc1 %3, $31\n" \
			insn " 1f\n" \
			"addiu %1, %1, 1\n" \
			"li %0, 0\n" \
			"j 2f\n" \
			"nop\n" \
			"1:\n" \
			"li %0, 1\n" \
			"2:\n" \
			".set reorder\n" \
			: "=r" (t), "+r" (s) : "r" (oldFcr31), "r" (fcr31) : "memory" \
		); \
		*taken = t; \
		*slot = s; \
	}

HAZARD_CTC(bc1f_c, "bc1f")
HAZARD_CTC(bc1t_c, "bc1t")

// cfc1 right after the compare, then again after a nop.
static void cfc1_after_compare(unsigned int a, unsigned int b, unsigned int oldFcr31, unsigned int *now, unsigned int *later) {
	unsigned int n, l;
	asm volatile (
		".set noreorder\n"
		"mtc1 %2, $f0\n"
		"mtc1 %3, $f1\n"
		"ctc1 %4, $31\n"
		"nop\n"
		"nop\n"
		"c.eq.s $f0, $f1\n"
		"cfc1 %0, $31\n"
		"nop\n"
		"cfc1 %1, $31\n"
		".set reorder\n"
		: "=r" (n), "=r" (l) : "r" (a), "r" (b), "r" (oldFcr31) : "$f0", "$f1", "memory"
	);
	*now = n;
	*later = l;
}

// mtc1 right before the compare that reads it.
static unsigned int compare_after_mtc1(unsigned int a, unsigned int b, unsigned int oldA) {
	unsigned int r;
	asm volatile (
		".set noreorder\n"
		"mtc1 %3, $f0\n"
		"mtc1 %2, $f1\n"
		"nop\n"
		"nop\n"
		"mtc1 %1, $f0\n"
		"c.eq.s $f0, $f1\n"
		"nop\n"
		"nop\n"
		"cfc1 %0, $31\n"
		".set reorder\n"
		: "=r" (r) : "r" (a), "r" (b), "r" (oldA) : "$f0", "$f1", "memory"
	);
	return r;
}

int main(int argc, char *argv[]) {
	int taken, slot;
	unsigned int now, later;
	static const struct { const char *name; void (*f)(unsigned int, unsigned int, unsigned int, int *, int *); } branches[] = {
		{ "bc1f", bc1f_h }, { "bc1t", bc1t_h }, { "bc1fl", bc1fl_h }, { "bc1tl", bc1tl_h },
	};

	printf("-- c.eq.s then the branch, condition previously the opposite --\n");
	for (int i = 0; i < ARRAY_SIZE(branches); i++) {
		branches[i].f(0x3f800000, 0x3f800000, 0x00000000, &taken, &slot);
		printf("%-6s 1 == 1 (was false): taken=%d slot=%d\n", branches[i].name, taken, slot);
		branches[i].f(0x3f800000, 0x40000000, 0x00800000, &taken, &slot);
		printf("%-6s 1 == 2 (was true): taken=%d slot=%d\n", branches[i].name, taken, slot);
	}

	printf("-- ctc1 then the branch --\n");
	bc1t_c(0x00000000, 0x00800000, &taken, &slot);
	printf("bc1t set (was clear): taken=%d slot=%d\n", taken, slot);
	bc1t_c(0x00800000, 0x00000000, &taken, &slot);
	printf("bc1t clear (was set): taken=%d slot=%d\n", taken, slot);
	bc1f_c(0x00800000, 0x00000000, &taken, &slot);
	printf("bc1f clear (was set): taken=%d slot=%d\n", taken, slot);
	bc1f_c(0x00000000, 0x00800000, &taken, &slot);
	printf("bc1f set (was clear): taken=%d slot=%d\n", taken, slot);

	printf("-- cfc1 right after c.eq.s --\n");
	cfc1_after_compare(0x3f800000, 0x3f800000, 0x00000000, &now, &later);
	printf("1 == 1 (was false): now=%08x later=%08x\n", now, later);
	cfc1_after_compare(0x3f800000, 0x40000000, 0x00800000, &now, &later);
	printf("1 == 2 (was true): now=%08x later=%08x\n", now, later);

	printf("-- c.eq.s right after mtc1 --\n");
	printf("f0 := 1 (was 2), f1 = 1: fcr31=%08x\n", compare_after_mtc1(0x3f800000, 0x3f800000, 0x40000000));
	printf("f0 := 2 (was 1), f1 = 1: fcr31=%08x\n", compare_after_mtc1(0x40000000, 0x3f800000, 0x3f800000));
	return 0;
}
