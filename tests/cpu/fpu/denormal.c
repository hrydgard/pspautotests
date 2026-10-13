#include <common.h>
#include <string.h>

// Denormal inputs and results, with flush-to-zero (fcr31 FS, bit 24) clear and set. Host FPUs
// disagree on this: ARM's FZ flushes inputs and outputs, x86's FTZ only outputs, and the two decide
// whether a result is tiny before or after rounding differently, which matters for results that
// round up to FLT_MIN. The arithmetic runs in all four rounding modes for that reason.

#define FS 0x01000000

static void setFcr31(unsigned int v) {
	asm volatile ("ctc1 %0, $31\nnop\n" : : "r" (v));
}

#define UNOP(name, insn) \
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

UNOP(sqrt_s, "sqrt.s")
UNOP(abs_s, "abs.s")
UNOP(neg_s, "neg.s")
UNOP(mov_s, "mov.s")
UNOP(cvt_w_s, "cvt.w.s")
UNOP(ceil_w_s, "ceil.w.s")
UNOP(floor_w_s, "floor.w.s")

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

#define CMP(cond) \
	static unsigned int c_##cond(unsigned int a, unsigned int b) { \
		unsigned int r; \
		asm volatile ( \
			"mtc1 %1, $f0\n" \
			"mtc1 %2, $f1\n" \
			"nop\n" \
			"c." #cond ".s $f0, $f1\n" \
			"nop\n" \
			"cfc1 %0, $31\n" \
			: "=r" (r) : "r" (a), "r" (b) : "$f0", "$f1", "memory" \
		); \
		return (r >> 23) & 1; \
	}

CMP(f) CMP(un) CMP(eq) CMP(ueq) CMP(olt) CMP(ult) CMP(ole) CMP(ule)
CMP(sf) CMP(ngle) CMP(seq) CMP(ngl) CMP(lt) CMP(nge) CMP(le) CMP(ngt)

typedef unsigned int (*CmpFunc)(unsigned int, unsigned int);
static const CmpFunc cmps[16] = {
	c_f, c_un, c_eq, c_ueq, c_olt, c_ult, c_ole, c_ule,
	c_sf, c_ngle, c_seq, c_ngl, c_lt, c_nge, c_le, c_ngt,
};

static const unsigned int unaryValues[] = {
	0x00000001,  // smallest denormal
	0x80000001,
	0x00400000,
	0x007fffff,  // largest denormal
	0x807fffff,
	0x00800000,  // FLT_MIN
	0x80800000,
};

static const unsigned int arithOps[][2] = {
	{ 0x00000001, 0x00000000 },  // denormal + 0
	{ 0x80000001, 0x00000000 },
	{ 0x00000001, 0x80000000 },
	{ 0x007fffff, 0x00000001 },  // two denormals that add to FLT_MIN
	{ 0x00400000, 0x00400000 },
	{ 0x00400000, 0x80400000 },
	{ 0x00800000, 0x00000001 },  // FLT_MIN and a denormal
	{ 0x00800001, 0x00800000 },  // two normals 1 ulp apart: sub gives a denormal
	{ 0x00400000, 0x3f800000 },  // denormal, 1
	{ 0x80400000, 0x3f800000 },
	{ 0x00400000, 0x4b800000 },  // denormal, 2^24: mul gives a normal
	{ 0x00800000, 0x3f000000 },  // FLT_MIN, 0.5: mul gives a denormal
	{ 0x80800000, 0x3f000000 },
	{ 0x00ffffff, 0x3f000000 },  // mul is tiny before rounding, FLT_MIN after (in nearest)
	{ 0x80ffffff, 0x3f000000 },
	{ 0x00fffffe, 0x3f000000 },  // mul is an exact denormal
	{ 0x00800001, 0x3f000000 },  // mul is half-way between two denormals
	{ 0x00800003, 0x3f000000 },
	{ 0x3f800000, 0x7f000000 },  // 1 / 2^127: div gives a denormal
	{ 0x3f800001, 0x7e800000 },  // div is just above FLT_MIN before rounding
	{ 0x3f7fffff, 0x7e800000 },  // div is just below FLT_MIN before rounding
	{ 0x3f800000, 0x00400000 },  // 1 / denormal
	{ 0x00000000, 0x00400000 },  // 0 / denormal
};

static const unsigned int cmpOps[][2] = {
	{ 0x00000001, 0x00000000 },  // denormal, 0
	{ 0x00000001, 0x80000000 },  // denormal, -0
	{ 0x80000001, 0x00000000 },  // -denormal, 0
	{ 0x00000001, 0x80000001 },  // denormal, -denormal
	{ 0x00000001, 0x007fffff },  // two denormals
	{ 0x007fffff, 0x00800000 },  // denormal, FLT_MIN
};

static void runArith(unsigned int mode) {
	for (int i = 0; i < ARRAY_SIZE(arithOps); i++) {
		unsigned int a = arithOps[i][0], b = arithOps[i][1];
		setFcr31(mode);
		unsigned int add = add_s(a, b);
		unsigned int sub = sub_s(a, b);
		unsigned int mul = mul_s(a, b);
		unsigned int div = div_s(a, b);
		printf("%08x %08x: add %08x sub %08x mul %08x div %08x\n", a, b, add, sub, mul, div);
	}
}

int main(int argc, char *argv[]) {
	for (int fs = 0; fs < 2; fs++) {
		unsigned int base = fs ? FS : 0;
		printf("==== FS=%d ====\n", fs);

		printf("-- unary --\n");
		for (int i = 0; i < ARRAY_SIZE(unaryValues); i++) {
			unsigned int v = unaryValues[i];
			setFcr31(base);
			unsigned int sqrt = sqrt_s(v), abs = abs_s(v), neg = neg_s(v), mov = mov_s(v);
			unsigned int cvt = cvt_w_s(v), ceil = ceil_w_s(v), floor = floor_w_s(v);
			printf("%08x: sqrt %08x abs %08x neg %08x mov %08x cvt.w %08x ceil.w %08x floor.w %08x\n",
				v, sqrt, abs, neg, mov, cvt, ceil, floor);
		}

		printf("-- compare (c.f .. c.ngt) --\n");
		for (int i = 0; i < ARRAY_SIZE(cmpOps); i++) {
			unsigned int a = cmpOps[i][0], b = cmpOps[i][1];
			setFcr31(base);
			unsigned int mask = 0;
			for (int c = 0; c < 16; c++) {
				mask |= cmps[c](a, b) << c;
			}
			printf("%08x %08x: %04x\n", a, b, mask);
		}

		for (unsigned int mode = 0; mode < 4; mode++) {
			printf("-- arithmetic, mode %u --\n", mode);
			runArith(base | mode);
		}
	}
	setFcr31(0);
	return 0;
}
