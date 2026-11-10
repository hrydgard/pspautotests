#include <common.h>

// A branch or jump (the inner) in the delay slot of another (the outer), and chains of three.
// MIPS calls this unpredictable; this records what the Allegrex does.
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
//           THIRD  -> C   (fall1 in the two-deep cases)
//           fall2, b done, nop, nop
//   A:      A1, A2, b done
//   B:      B1, B2, b done
//   C:      C1, C2, b done
// Each marker sets one bit, so the output shows which instructions ran. ra starts at 0, and is
// printed relative to the outer instruction.
//
// The FPU condition and VFPU CC bit 0 are set, so bc1t and bvt are taken, bc1f and bvf not.
// $t8, $t7 and $t4 hold A, B and C, for the register jumps.

enum {
	FALL1 = 1,
	FALL2 = 2,
	A1 = 4,
	A2 = 8,
	B1 = 16,
	B2 = 32,
	C1 = 64,
	C2 = 128,
};

#define FALL1_OP "ori $t0, $t0, 1"

#define CASE3(OUTER, INNER, THIRD) { \
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
		"la $t4, 5f\n" \
		"la $t6, 4f\n" \
		"move $ra, $0\n" \
		"4:\n" \
		OUTER "\n" \
		INNER "\n" \
		THIRD "\n" \
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
		"5:\n" \
		"ori $t0, $t0, 64\n" \
		"ori $t0, $t0, 128\n" \
		"b 3f\n" \
		"nop\n" \
		"3:\n" \
		"move %0, $t0\n" \
		"move %1, $ra\n" \
		"move %2, $t6\n" \
		"move $ra, $t9\n" \
		".set pop\n" \
		: "=r"(ran), "=r"(ra), "=r"(base) : : "t0", "t4", "t5", "t6", "t7", "t8", "t9", "$f0", "$f1", "memory" \
	); \
	printResult(OUTER, INNER, THIRD, ran, ra, base); \
}

#define CASE(OUTER, INNER) CASE3(OUTER, INNER, FALL1_OP)

static void printResult(const char *outer, const char *inner, const char *third, u32 ran, u32 ra, u32 base) {
	char desc[96];
	if (strcmp(third, FALL1_OP) == 0) {
		snprintf(desc, sizeof(desc), "%s / %s:", outer, inner);
	} else {
		snprintf(desc, sizeof(desc), "%s / %s / %s:", outer, inner, third);
	}
	printf("  %-40s", desc);
	printf(" %s", (ran & FALL1) ? "fall1" : "-----");
	printf(" %s", (ran & FALL2) ? "fall2" : "-----");
	printf(" %s", (ran & A1) ? "A1" : "--");
	printf(" %s", (ran & A2) ? "A2" : "--");
	printf(" %s", (ran & B1) ? "B1" : "--");
	printf(" %s", (ran & B2) ? "B2" : "--");
	printf(" %s", (ran & C1) ? "C1" : "--");
	printf(" %s", (ran & C2) ? "C2" : "--");
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

// Third in the chain, after an outer and inner that each go somewhere different.
#define CHAIN_CASES(OUTER, INNER) \
	CASE3(OUTER, INNER, "j 5f"); \
	CASE3(OUTER, INNER, "jal 5f"); \
	CASE3(OUTER, INNER, "jr $t4"); \
	CASE3(OUTER, INNER, "beq $0, $0, 5f"); \
	CASE3(OUTER, INNER, "bne $0, $0, 5f"); \
	CASE3(OUTER, INNER, "beql $0, $0, 5f"); \
	CASE3(OUTER, INNER, "bnel $0, $0, 5f"); \
	CASE3(OUTER, INNER, "bgezal $0, 5f");

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

	printf("chains:\n");
	CHAIN_CASES("j 1f", "j 2f");
	CHAIN_CASES("j 1f", "beq $0, $0, 2f");
	CHAIN_CASES("j 1f", "bnel $0, $0, 2f");
	CHAIN_CASES("jal 1f", "jal 2f");
	CHAIN_CASES("beq $0, $0, 1f", "j 2f");
	CHAIN_CASES("beq $0, $0, 1f", "beq $0, $0, 2f");
	CHAIN_CASES("beql $0, $0, 1f", "beql $0, $0, 2f");
	CHAIN_CASES("bne $0, $0, 1f", "j 2f");
	CHAIN_CASES("bne $0, $0, 1f", "beq $0, $0, 2f");
	CHAIN_CASES("bnel $0, $0, 1f", "j 2f");
	CHAIN_CASES("bnel $0, $0, 1f", "beq $0, $0, 2f");

	return 0;
}
