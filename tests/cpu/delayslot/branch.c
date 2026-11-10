#include <common.h>

// A branch or jump (the inner) in the delay slot of another (the outer). MIPS calls this
// unpredictable; this records what the Allegrex does.
//
// What it does (6.60), for an outer O with an inner I in its delay slot:
// - O a taken jump: if I is a taken branch or a jump, I's target runs next and nothing at O's
//   target does. I links to O's target.
// - O a taken conditional branch: a jump in I is ignored (it still links, to I + 8). A taken
//   conditional I goes to its own target.
// - O not taken: I runs as usual. If O is likely, I is annulled - except that an annulled jump
//   still jumps, with its delay slot, and only its link is lost.
//
// Layout of each case:
//   outer:  OUTER  -> A
//           INNER  -> B
//           fall1, fall2, b done, nop, nop
//   A:      A1, A2, b done
//   B:      B1, B2, b done
// Each marker sets one bit, so the output shows which instructions ran. ra starts at 0, and is
// printed relative to the outer instruction.
//
// The FPU condition and VFPU CC bit 0 are set, so bc1t and bvt are taken, bc1f and bvf not.
// $t8 and $t7 hold A and B, for the register jumps.

enum {
	FALL1 = 1,
	FALL2 = 2,
	A1 = 4,
	A2 = 8,
	B1 = 16,
	B2 = 32,
};

#define CASE(OUTER, INNER) { \
	u32 ran, ra, base; \
	asm volatile ( \
		".set push\n" \
		".set noreorder\n" \
		"mtc1 $0, $f0\n" \
		"mtc1 $0, $f1\n" \
		"nop\n" \
		"c.eq.s $f0, $f1\n" \
		"li $t5, 1\n" \
		"mtvc $t5, $131\n" \
		"nop\n" \
		"nop\n" \
		"nop\n" \
		"nop\n" \
		"nop\n" \
		"nop\n" \
		"nop\n" \
		"nop\n" \
		"move $t9, $ra\n" \
		"move $t0, $0\n" \
		"la $t8, 1f\n" \
		"la $t7, 2f\n" \
		"la $t6, 4f\n" \
		"move $ra, $0\n" \
		"4:\n" \
		OUTER "\n" \
		INNER "\n" \
		"ori $t0, $t0, 1\n" \
		"ori $t0, $t0, 2\n" \
		"b 3f\n" \
		"nop\n" \
		"nop\n" \
		"nop\n" \
		"1:\n" \
		"ori $t0, $t0, 4\n" \
		"ori $t0, $t0, 8\n" \
		"b 3f\n" \
		"nop\n" \
		"2:\n" \
		"ori $t0, $t0, 16\n" \
		"ori $t0, $t0, 32\n" \
		"b 3f\n" \
		"nop\n" \
		"3:\n" \
		"move %0, $t0\n" \
		"move %1, $ra\n" \
		"move %2, $t6\n" \
		"move $ra, $t9\n" \
		".set pop\n" \
		: "=r"(ran), "=r"(ra), "=r"(base) : : "t0", "t5", "t6", "t7", "t8", "t9", "$f0", "$f1", "memory" \
	); \
	printResult(OUTER, INNER, ran, ra, base); \
}

static void printResult(const char *outer, const char *inner, u32 ran, u32 ra, u32 base) {
	char desc[64];
	snprintf(desc, sizeof(desc), "%s / %s:", outer, inner);
	printf("  %-36s", desc);
	printf(" %s", (ran & FALL1) ? "fall1" : "-----");
	printf(" %s", (ran & FALL2) ? "fall2" : "-----");
	printf(" %s", (ran & A1) ? "A1" : "--");
	printf(" %s", (ran & A2) ? "A2" : "--");
	printf(" %s", (ran & B1) ? "B1" : "--");
	printf(" %s", (ran & B2) ? "B2" : "--");
	if (ra == 0) {
		printf("  ra unset\n");
	} else {
		printf("  ra outer%+d\n", (int)(ra - base));
	}
}

#define OUTER_CASES(OUTER) \
	printf("%s:\n", OUTER); \
	CASE(OUTER, "j 2f"); \
	CASE(OUTER, "jal 2f"); \
	CASE(OUTER, "jr $t7"); \
	CASE(OUTER, "jalr $t7"); \
	CASE(OUTER, "beq $0, $0, 2f"); \
	CASE(OUTER, "bne $0, $0, 2f"); \
	CASE(OUTER, "beql $0, $0, 2f"); \
	CASE(OUTER, "bnel $0, $0, 2f"); \
	CASE(OUTER, "bgezal $0, 2f"); \
	CASE(OUTER, "bltzal $0, 2f"); \
	CASE(OUTER, "bgezall $0, 2f"); \
	CASE(OUTER, "bltzall $0, 2f"); \
	CASE(OUTER, "bc1t 2f"); \
	CASE(OUTER, "bc1f 2f"); \
	CASE(OUTER, "bc1tl 2f"); \
	CASE(OUTER, "bc1fl 2f"); \
	CASE(OUTER, "bvt 0, 2f"); \
	CASE(OUTER, "bvf 0, 2f"); \
	CASE(OUTER, "bvtl 0, 2f"); \
	CASE(OUTER, "bvfl 0, 2f"); \
	printf("\n");

int main(int argc, char **argv) {
	// Taken.
	OUTER_CASES("j 1f");
	OUTER_CASES("jal 1f");
	OUTER_CASES("jr $t8");
	OUTER_CASES("jalr $t8");
	OUTER_CASES("beq $0, $0, 1f");
	OUTER_CASES("beql $0, $0, 1f");
	OUTER_CASES("bgezal $0, 1f");
	OUTER_CASES("bgezall $0, 1f");
	OUTER_CASES("bc1t 1f");
	OUTER_CASES("bc1tl 1f");
	OUTER_CASES("bvt 0, 1f");
	OUTER_CASES("bvtl 0, 1f");
	// Not taken.
	OUTER_CASES("bne $0, $0, 1f");
	OUTER_CASES("bnel $0, $0, 1f");
	OUTER_CASES("bltzal $0, 1f");
	OUTER_CASES("bltzall $0, 1f");
	OUTER_CASES("bc1f 1f");
	OUTER_CASES("bc1fl 1f");
	OUTER_CASES("bvf 0, 1f");
	OUTER_CASES("bvfl 0, 1f");

	return 0;
}
