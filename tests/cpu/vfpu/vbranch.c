#include <common.h>

// NOTE: The VFPU pipeline is deep. A CC written with mtvc isn't seen by a branch until about six
// instructions later, and mfvc right after a vcmp reads the old value (vbranch_hazard.c measures
// both), so everything here pads with nops after an mtvc and before an mfvc.
//
// The VFPU condition branches bvt/bvf/bvtl/bvfl. Which CC bit each imm3 tests, whether the
// likely variants skip the delay slot when not taken, what a vcmp or a CC write in the delay slot
// or right before the branch does to the outcome, and what the imm3 values past the six CC bits
// test. Each case reports two things: whether the branch was taken, and whether the delay slot
// instruction ran.

// Set CC to cc, then "branch imm, target" with an addiu in the delay slot.
// Returns (taken << 1) | delaySlotRan.
#define BRANCH_CASE(name, branch, imm) \
	static int __attribute__((noinline)) name(unsigned int cc) { \
		int taken, slot; \
		asm volatile ( \
			".set noreorder\n" \
			"mtvc   %2, $131\n" \
			"nop\n" \
			"nop\n" \
			"nop\n" \
			"nop\n" \
			"nop\n" \
			"nop\n" \
			"nop\n" \
			"nop\n" \
			"li     %0, 0\n" \
			"li     %1, 0\n" \
			branch " " #imm ", 1f\n" \
			"li     %1, 1\n" \
			"j      2f\n" \
			"nop\n" \
			"1:\n" \
			"li     %0, 1\n" \
			"2:\n" \
			".set reorder\n" \
			: "=&r" (taken), "=&r" (slot) : "r" (cc) : "memory" \
		); \
		return (taken << 1) | slot; \
	}

BRANCH_CASE(bvt0, "bvt", 0)  BRANCH_CASE(bvt1, "bvt", 1)  BRANCH_CASE(bvt2, "bvt", 2)  BRANCH_CASE(bvt3, "bvt", 3)
BRANCH_CASE(bvt4, "bvt", 4)  BRANCH_CASE(bvt5, "bvt", 5)  BRANCH_CASE(bvt6, "bvt", 6)  BRANCH_CASE(bvt7, "bvt", 7)
BRANCH_CASE(bvf0, "bvf", 0)  BRANCH_CASE(bvf1, "bvf", 1)  BRANCH_CASE(bvf2, "bvf", 2)  BRANCH_CASE(bvf3, "bvf", 3)
BRANCH_CASE(bvf4, "bvf", 4)  BRANCH_CASE(bvf5, "bvf", 5)  BRANCH_CASE(bvf6, "bvf", 6)  BRANCH_CASE(bvf7, "bvf", 7)
BRANCH_CASE(bvtl0, "bvtl", 0)  BRANCH_CASE(bvtl1, "bvtl", 1)  BRANCH_CASE(bvtl2, "bvtl", 2)  BRANCH_CASE(bvtl3, "bvtl", 3)
BRANCH_CASE(bvtl4, "bvtl", 4)  BRANCH_CASE(bvtl5, "bvtl", 5)  BRANCH_CASE(bvtl6, "bvtl", 6)  BRANCH_CASE(bvtl7, "bvtl", 7)
BRANCH_CASE(bvfl0, "bvfl", 0)  BRANCH_CASE(bvfl1, "bvfl", 1)  BRANCH_CASE(bvfl2, "bvfl", 2)  BRANCH_CASE(bvfl3, "bvfl", 3)
BRANCH_CASE(bvfl4, "bvfl", 4)  BRANCH_CASE(bvfl5, "bvfl", 5)  BRANCH_CASE(bvfl6, "bvfl", 6)  BRANCH_CASE(bvfl7, "bvfl", 7)

typedef int (*BranchFunc)(unsigned int);
static const BranchFunc bvt[8] = { bvt0, bvt1, bvt2, bvt3, bvt4, bvt5, bvt6, bvt7 };
static const BranchFunc bvf[8] = { bvf0, bvf1, bvf2, bvf3, bvf4, bvf5, bvf6, bvf7 };
static const BranchFunc bvtl[8] = { bvtl0, bvtl1, bvtl2, bvtl3, bvtl4, bvtl5, bvtl6, bvtl7 };
static const BranchFunc bvfl[8] = { bvfl0, bvfl1, bvfl2, bvfl3, bvfl4, bvfl5, bvfl6, bvfl7 };

// A vcmp in the delay slot that flips the tested bit. CC starts as "cc"; the vcmp.s EQ on two
// equal values sets bit 0 (and 4, 5), on two different ones clears them.
#define SLOT_VCMP_CASE(name, branch, imm) \
	static int __attribute__((noinline)) name(unsigned int cc, float a, float b) { \
		int taken; \
		asm volatile ( \
			".set noreorder\n" \
			"mtv    %2, S000\n" \
			"mtv    %3, S001\n" \
			"mtvc   %1, $131\n" \
			"nop\n" \
			"nop\n" \
			"nop\n" \
			"nop\n" \
			"nop\n" \
			"nop\n" \
			"nop\n" \
			"nop\n" \
			"li     %0, 0\n" \
			branch " " #imm ", 1f\n" \
			"vcmp.s EQ, S000, S001\n" \
			"j      2f\n" \
			"nop\n" \
			"1:\n" \
			"li     %0, 1\n" \
			"2:\n" \
			".set reorder\n" \
			: "=&r" (taken) : "r" (cc), "r" (a), "r" (b) : "memory" \
		); \
		return taken; \
	}

SLOT_VCMP_CASE(slot_bvt0, "bvt", 0)
SLOT_VCMP_CASE(slot_bvf0, "bvf", 0)
SLOT_VCMP_CASE(slot_bvt4, "bvt", 4)
SLOT_VCMP_CASE(slot_bvtl0, "bvtl", 0)
SLOT_VCMP_CASE(slot_bvfl0, "bvfl", 0)

// The CC read back after a branch with a vcmp in the delay slot: did the vcmp run (on both paths)?
static unsigned int __attribute__((noinline)) slot_cc_after(unsigned int cc, float a, float b, int useLikely) {
	unsigned int after;
	if (useLikely) {
		asm volatile (
			".set noreorder\n"
			"mtv    %1, S000\n"
			"mtv    %2, S001\n"
			"mtvc   %3, $131\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"bvtl   0, 1f\n"
			"vcmp.s EQ, S000, S001\n"
			"1:\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"mfvc   %0, $131\n"
			".set reorder\n"
			: "=&r" (after) : "r" (a), "r" (b), "r" (cc) : "memory"
		);
	} else {
		asm volatile (
			".set noreorder\n"
			"mtv    %1, S000\n"
			"mtv    %2, S001\n"
			"mtvc   %3, $131\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"bvt    0, 1f\n"
			"vcmp.s EQ, S000, S001\n"
			"1:\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"nop\n"
			"mfvc   %0, $131\n"
			".set reorder\n"
			: "=&r" (after) : "r" (a), "r" (b), "r" (cc) : "memory"
		);
	}
	return after;
}

// A vcmp immediately before the branch (the usual pattern), with no instruction between.
static int __attribute__((noinline)) vcmp_then_branch(float a, float b, int useBvt) {
	int taken;
	if (useBvt) {
		asm volatile (
			".set noreorder\n"
			"mtv    %1, S000\n"
			"mtv    %2, S001\n"
			"li     %0, 0\n"
			"vcmp.s LT, S000, S001\n"
			"bvt    0, 1f\n"
			"nop\n"
			"j      2f\n"
			"nop\n"
			"1:\n"
			"li     %0, 1\n"
			"2:\n"
			".set reorder\n"
			: "=&r" (taken) : "r" (a), "r" (b) : "memory"
		);
	} else {
		asm volatile (
			".set noreorder\n"
			"mtv    %1, S000\n"
			"mtv    %2, S001\n"
			"li     %0, 0\n"
			"vcmp.s LT, S000, S001\n"
			"bvf    0, 1f\n"
			"nop\n"
			"j      2f\n"
			"nop\n"
			"1:\n"
			"li     %0, 1\n"
			"2:\n"
			".set reorder\n"
			: "=&r" (taken) : "r" (a), "r" (b) : "memory"
		);
	}
	return taken;
}

// vcmp.q sets bits 0-3 per lane, 4 = any, 5 = all. Read each through a branch.
static unsigned int __attribute__((noinline)) vcmp_q_bits(const float *a, const float *b) {
	unsigned int bits = 0;
	asm volatile (
			".set noreorder\n"
		"lv.q   C100, 0(%1)\n"
		"lv.q   C110, 0(%2)\n"
		"vcmp.q LT, C100, C110\n"
		"bvf    0, 1f\n"
		"nop\n"
		"ori    %0, %0, 1\n"
		"1:\n"
		"bvf    1, 2f\n"
		"nop\n"
		"ori    %0, %0, 2\n"
		"2:\n"
		"bvf    2, 3f\n"
		"nop\n"
		"ori    %0, %0, 4\n"
		"3:\n"
		"bvf    3, 4f\n"
		"nop\n"
		"ori    %0, %0, 8\n"
		"4:\n"
		"bvf    4, 5f\n"
		"nop\n"
		"ori    %0, %0, 16\n"
		"5:\n"
		"bvf    5, 6f\n"
		"nop\n"
		"ori    %0, %0, 32\n"
		"6:\n"
			".set reorder\n"
		: "+r" (bits) : "r" (a), "r" (b) : "memory"
	);
	return bits;
}

static const char *describe(int r) {
	switch (r) {
	case 0: return "not taken, slot skipped";
	case 1: return "not taken, slot ran";
	case 2: return "taken, slot skipped";
	case 3: return "taken, slot ran";
	}
	return "?";
}

int main(int argc, char *argv[]) {
	// The first VFPU instruction in a process goes through the lazy coprocessor enable, which
	// throws the timing of everything around it. Get that over with.
	asm volatile (".set noreorder\n" "mtvc $zero, $131\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nvnop\nmfvc $t0, $131\n" ".set reorder\n" : : : "t0", "memory");

	static const unsigned int ccs[] = { 0x00, 0x3F, 0x01, 0x20, 0x15, 0x2A, 0xC0, 0xFF };
	for (int c = 0; c < ARRAY_SIZE(ccs); c++) {
		printf("-- CC = %02x --\n", ccs[c]);
		for (int i = 0; i < 8; i++) {
			printf("imm %d: bvt %-24s bvf %-24s bvtl %-24s bvfl %s\n", i,
				describe(bvt[i](ccs[c])), describe(bvf[i](ccs[c])), describe(bvtl[i](ccs[c])), describe(bvfl[i](ccs[c])));
		}
	}

	printf("-- vcmp in the delay slot --\n");
	printf("CC=0, bvt 0, slot sets bit:    %s\n", slot_bvt0(0x00, 1.0f, 1.0f) ? "taken" : "not taken");
	printf("CC=3f, bvt 0, slot clears bit: %s\n", slot_bvt0(0x3F, 1.0f, 2.0f) ? "taken" : "not taken");
	printf("CC=3f, bvf 0, slot clears bit: %s\n", slot_bvf0(0x3F, 1.0f, 2.0f) ? "taken" : "not taken");
	printf("CC=0, bvf 0, slot sets bit:    %s\n", slot_bvf0(0x00, 1.0f, 1.0f) ? "taken" : "not taken");
	printf("CC=0, bvt 4, slot sets bit:    %s\n", slot_bvt4(0x00, 1.0f, 1.0f) ? "taken" : "not taken");
	printf("CC=0, bvtl 0, slot sets bit:   %s\n", slot_bvtl0(0x00, 1.0f, 1.0f) ? "taken" : "not taken");
	printf("CC=3f, bvfl 0, slot clears bit: %s\n", slot_bvfl0(0x3F, 1.0f, 2.0f) ? "taken" : "not taken");
	printf("CC after bvt (not taken) with slot vcmp EQ true:  %02x\n", slot_cc_after(0x00, 1.0f, 1.0f, 0));
	printf("CC after bvt (taken) with slot vcmp EQ false:     %02x\n", slot_cc_after(0x3F, 1.0f, 2.0f, 0));
	printf("CC after bvtl (not taken) with slot vcmp EQ true: %02x\n", slot_cc_after(0x00, 1.0f, 1.0f, 1));
	printf("CC after bvtl (taken) with slot vcmp EQ false:    %02x\n", slot_cc_after(0x3F, 1.0f, 2.0f, 1));

	printf("-- vcmp immediately before the branch --\n");
	printf("1 < 2, bvt: %s\n", vcmp_then_branch(1.0f, 2.0f, 1) ? "taken" : "not taken");
	printf("2 < 1, bvt: %s\n", vcmp_then_branch(2.0f, 1.0f, 1) ? "taken" : "not taken");
	printf("1 < 2, bvf: %s\n", vcmp_then_branch(1.0f, 2.0f, 0) ? "taken" : "not taken");
	printf("2 < 1, bvf: %s\n", vcmp_then_branch(2.0f, 1.0f, 0) ? "taken" : "not taken");

	printf("-- vcmp.q LT lane bits through branches --\n");
	static const float a1[4] __attribute__((aligned(16))) = { 1.0f, 5.0f, 1.0f, 5.0f };
	static const float b1[4] __attribute__((aligned(16))) = { 2.0f, 2.0f, 2.0f, 2.0f };
	static const float a2[4] __attribute__((aligned(16))) = { 1.0f, 1.0f, 1.0f, 1.0f };
	static const float a3[4] __attribute__((aligned(16))) = { 5.0f, 5.0f, 5.0f, 5.0f };
	printf("[1,5,1,5] < 2: %02x\n", vcmp_q_bits(a1, b1));
	printf("[1,1,1,1] < 2: %02x\n", vcmp_q_bits(a2, b1));
	printf("[5,5,5,5] < 2: %02x\n", vcmp_q_bits(a3, b1));
	return 0;
}
