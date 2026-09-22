#include <common.h>
#include <string.h>

// The FPU under each of the four rounding modes in fcr31 (0 nearest-even, 1 toward zero, 2 up,
// 3 down): which conversions follow the mode and which have it fixed, what comes out of a
// float-to-int conversion at and past the edges of the int32 range and for NaN/inf, how
// int-to-float rounds an int that doesn't fit in 24 bits, and the last bit of the arithmetic ops
// on inexact results.

typedef union { float f; unsigned int u; int i; } FU;

static void setMode(unsigned int mode) {
	asm volatile ("ctc1 %0, $31" : : "r" (mode));
}

#define TO_INT(name, insn) \
	static unsigned int name(unsigned int x) { \
		unsigned int r; \
		asm volatile ( \
			"mtc1 %1, $f0\n" \
			"nop\n" \
			insn " $f1, $f0\n" \
			"nop\n" \
			"mfc1 %0, $f1\n" \
			: "=r" (r) : "r" (x) : "$f0", "$f1", "memory" \
		); \
		return r; \
	}

TO_INT(round_w_s, "round.w.s")
TO_INT(trunc_w_s, "trunc.w.s")
TO_INT(ceil_w_s, "ceil.w.s")
TO_INT(floor_w_s, "floor.w.s")
TO_INT(cvt_w_s, "cvt.w.s")
TO_INT(cvt_s_w, "cvt.s.w")
TO_INT(sqrt_s, "sqrt.s")

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

static const unsigned int toIntValues[] = {
	0x3f000000,  // 0.5
	0x3fc00000,  // 1.5
	0x40200000,  // 2.5
	0xbf000000,  // -0.5
	0xbfc00000,  // -1.5
	0xc0200000,  // -2.5
	0x3effffff,  // 0.49999997
	0x3f800001,  // 1.0000001
	0x406ccccd,  // 3.7
	0xc06ccccd,  // -3.7
	0x4b7fffff,  // 16777215 (odd, exact)
	0x4effffff,  // 2147483520, the largest float below 2^31
	0x4f000000,  // 2^31
	0x4f000001,  // 2147483904
	0xceffffff,  // -2147483520
	0xcf000000,  // -2^31
	0xcf000001,  // -2147483904
	0x4f800000,  // 2^32
	0x501502f9,  // 1e10
	0xd01502f9,  // -1e10
	0x7149f2ca,  // 1e30
	0x7f800000,  // inf
	0xff800000,  // -inf
	0x7fc00000,  // NaN
	0xffc00000,  // -NaN
	0x7f800001,  // a NaN with a different payload
	0x000116c2,  // denormal
	0x800116c2,  // -denormal
	0x00000000,
	0x80000000,
};

static const int toFloatValues[] = {
	0, 1, -1, 100,
	16777215, 16777216, 16777217, -16777217,   // 2^24 - 1 .. 2^24 + 1: the first one that doesn't fit
	16777219, -16777219,                       // 2^24 + 3, a tie that rounds the other way
	33554435, -33554435,                       // 2^25 + 3, half-way between representable values
	0x7fffffff, (int)0x80000000, 0x7fffffc0, 0x7fffffbf, 0x7fffff80, 0x7fffff81, (int)0x80000041,
	0x12345678, (int)0x87654321,
};

static const unsigned int arithOps[][2] = {
	{ 0x3f800000, 0x33800000 },  // 1 + 2^-24: half an ulp
	{ 0x3f800000, 0x33c00000 },  // 1 + 1.5 * 2^-24
	{ 0x3f800000, 0x34000000 },  // 1 + 2^-23: exact
	{ 0xbf800000, 0x33800000 },  // -1 + 2^-24
	{ 0x3f8ccccd, 0x3f8ccccd },  // 1.1, 1.1
	{ 0x3f800000, 0x40400000 },  // 1, 3
	{ 0x40000000, 0x40400000 },  // 2, 3
	{ 0xbf800000, 0x40400000 },  // -1, 3
	{ 0x40000000, 0x3f800000 },  // 2, 1
	{ 0x4b7fffff, 0x3f800000 },  // 16777215, 1
	{ 0x00800000, 0x3f000000 },  // FLT_MIN, 0.5: the result is a denormal
	{ 0x7f7fffff, 0x7f7fffff },  // FLT_MAX, FLT_MAX: overflow
};

int main(int argc, char *argv[]) {
	for (unsigned int mode = 0; mode < 4; mode++) {
		setMode(mode);
		printf("== mode %u ==\n", mode);
		printf("-- float to int --\n");
		for (int i = 0; i < ARRAY_SIZE(toIntValues); i++) {
			unsigned int v = toIntValues[i];
			printf("%08x: round %08x trunc %08x ceil %08x floor %08x cvt %08x\n", v,
				round_w_s(v), trunc_w_s(v), ceil_w_s(v), floor_w_s(v), cvt_w_s(v));
		}
		printf("-- int to float --\n");
		for (int i = 0; i < ARRAY_SIZE(toFloatValues); i++) {
			int v = toFloatValues[i];
			printf("%11d: cvt.s.w %08x\n", v, cvt_s_w((unsigned int)v));
		}
		printf("-- arithmetic --\n");
		for (int i = 0; i < ARRAY_SIZE(arithOps); i++) {
			unsigned int a = arithOps[i][0], b = arithOps[i][1];
			printf("%08x %08x: add %08x sub %08x mul %08x div %08x sqrt(a) %08x\n", a, b,
				add_s(a, b), sub_s(a, b), mul_s(a, b), div_s(a, b), sqrt_s(a));
		}
	}
	setMode(0);
	return 0;
}
