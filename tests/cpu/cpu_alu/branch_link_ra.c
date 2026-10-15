#include <common.h>

// bltzal and friends with ra as the register they test: is it tested before or after the link
// overwrites it? Prints whether the branch was taken and ra's offset from the link address. The
// assembler won't take ra there (MIPS calls it unpredictable), so they're words: REGIMM, rs = 31,
// rt the op, to target_ three instructions past the delay slot.

#define LINK_TEST(name, rt, value) \
	static void __attribute__((noinline)) name(int *taken, int *link) { \
		int t, r, base; \
		asm volatile ( \
			".set    noreorder\n" \
			"move    $t2, $ra\n" \
			"li      $ra, " #value "\n" \
			"li      %0, 0\n" \
			".word   0x07e00003 | (" #rt " << 16)\n" \
			"nop\n" \
			"after_%=:\n" \
			"j       done_%=\n" \
			"nop\n" \
			"target_%=:\n" \
			"li      %0, 1\n" \
			"done_%=:\n" \
			"move    %1, $ra\n" \
			"la      %2, after_%=\n" \
			"move    $ra, $t2\n" \
			".set    reorder\n" \
			: "=&r"(t), "=&r"(r), "=&r"(base) : : "t2", "memory" \
		); \
		*taken = t; \
		*link = r - base; \
	}

LINK_TEST(t_bltzal_neg, 0x10, -1)
LINK_TEST(t_bltzal_pos, 0x10, 1)
LINK_TEST(t_bgezal_neg, 0x11, -1)
LINK_TEST(t_bgezal_pos, 0x11, 1)
LINK_TEST(t_bltzall_neg, 0x12, -1)
LINK_TEST(t_bltzall_pos, 0x12, 1)
LINK_TEST(t_bgezall_neg, 0x13, -1)
LINK_TEST(t_bgezall_pos, 0x13, 1)

static const struct {
	const char *name;
	void (*func)(int *taken, int *link);
} tests[] = {
	{ "bltzal ra=-1", t_bltzal_neg }, { "bltzal ra=1", t_bltzal_pos },
	{ "bgezal ra=-1", t_bgezal_neg }, { "bgezal ra=1", t_bgezal_pos },
	{ "bltzall ra=-1", t_bltzall_neg }, { "bltzall ra=1", t_bltzall_pos },
	{ "bgezall ra=-1", t_bgezall_neg }, { "bgezall ra=1", t_bgezall_pos },
};

int main(int argc, char **argv) {
	for (size_t i = 0; i < ARRAY_SIZE(tests); i++) {
		int taken, link;
		tests[i].func(&taken, &link);
		printf("%-14s taken=%d ra=after%+d\n", tests[i].name, taken, link);
	}
	return 0;
}
