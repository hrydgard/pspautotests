#include <common.h>
#include <string.h>

// The NaN the FPU makes from scratch: 0/0, inf - inf, 0 * inf, inf / inf. It's the positive
// quiet NaN 0x7fc00000. x86 hosts produce the negative one (0xffc00000) for the same operations,
// and matching the sign would cost a check after every arithmetic op, so this is kept apart from
// roundmode.c. (sqrt of a negative is in roundmode.c: that one is cheap to fix.)

#define BINOP(name, insn) \
	static unsigned int name(unsigned int a, unsigned int b) { \
		unsigned int r; \
		asm volatile ( \
			"mtc1 %1, $f0\n" \
			"mtc1 %2, $f1\n" \
			"nop\n" \
			insn " $f2, $f0, $f1\n" \
			"nop\n" \
			"mfc1 %0, $f2\n" \
			: "=r" (r) : "r" (a), "r" (b) : "$f0", "$f1", "$f2", "memory" \
		); \
		return r; \
	}

BINOP(add_s, "add.s")
BINOP(sub_s, "sub.s")
BINOP(mul_s, "mul.s")
BINOP(div_s, "div.s")

static const unsigned int ops[][2] = {
	{ 0x00000000, 0x00000000 },  // 0, 0
	{ 0x80000000, 0x00000000 },  // -0, 0
	{ 0x7f800000, 0x7f800000 },  // inf, inf
	{ 0xff800000, 0x7f800000 },  // -inf, inf
	{ 0x7f800000, 0x00000000 },  // inf, 0
	{ 0x00000000, 0xff800000 },  // 0, -inf
};

int main(int argc, char *argv[]) {
	asm volatile ("ctc1 $0, $31");
	for (int i = 0; i < ARRAY_SIZE(ops); i++) {
		unsigned int a = ops[i][0], b = ops[i][1];
		printf("%08x %08x: add %08x sub %08x mul %08x div %08x\n", a, b,
			add_s(a, b), sub_s(a, b), mul_s(a, b), div_s(a, b));
	}
	return 0;
}
