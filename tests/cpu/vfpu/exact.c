#include <common.h>
#include "vfpu_common.h"
#include <string.h>

// The exact bits of the VFPU's unary math functions: vrcp, vrsq, vsqrt, vsin, vcos, vnsin, vasin,
// vexp2, vrexp2, vlog2 and vnrcp, over the special values, a sweep of exponents and mantissa
// patterns in both signs, and some random bit patterns. The hardware computes approximations, not
// the IEEE-rounded result; cpu/vfpu/vector only checks these to a few digits.

typedef union { float f[4]; unsigned int u[4]; } __attribute__((aligned(16))) UVec4;

#define UNARY(name, instr) \
	static void NOINLINE name(UVec4 *out, const UVec4 *in) { \
		asm volatile ( \
			"lv.q C100, %1\n" \
			instr " C000, C100\n" \
			"sv.q C000, %0\n" \
			: "=m" (*out) : "m" (*in) : "memory"); \
	}

UNARY(vrcp_q,   "vrcp.q")
UNARY(vrsq_q,   "vrsq.q")
UNARY(vsqrt_q,  "vsqrt.q")
UNARY(vsin_q,   "vsin.q")
UNARY(vcos_q,   "vcos.q")
UNARY(vnsin_q,  "vnsin.q")
UNARY(vasin_q,  "vasin.q")
UNARY(vexp2_q,  "vexp2.q")
UNARY(vrexp2_q, "vrexp2.q")
UNARY(vlog2_q,  "vlog2.q")
UNARY(vnrcp_q,  "vnrcp.q")

typedef void (*UnaryFunc)(UVec4 *, const UVec4 *);
static const struct { const char *name; UnaryFunc func; } ops[] = {
	{ "rcp", vrcp_q }, { "rsq", vrsq_q }, { "sqrt", vsqrt_q }, { "sin", vsin_q }, { "cos", vcos_q },
	{ "nsin", vnsin_q }, { "asin", vasin_q }, { "exp2", vexp2_q }, { "rexp2", vrexp2_q },
	{ "log2", vlog2_q }, { "nrcp", vnrcp_q },
};

static unsigned int inputs[512];
static int numInputs;

static void add(unsigned int v) {
	if (numInputs < ARRAY_SIZE(inputs)) {
		inputs[numInputs++] = v;
	}
}

static void run(const char *title) {
	printf("-- %s --\n", title);
	for (int i = 0; i < numInputs; i += 4) {
		UVec4 in, r[ARRAY_SIZE(ops)];
		memset(&in, 0, sizeof(in));
		for (int lane = 0; lane < 4 && i + lane < numInputs; lane++) {
			in.u[lane] = inputs[i + lane];
		}
		for (int o = 0; o < ARRAY_SIZE(ops); o++) {
			ops[o].func(&r[o], &in);
		}
		for (int lane = 0; lane < 4 && i + lane < numInputs; lane++) {
			printf("%08x:", in.u[lane]);
			for (int o = 0; o < ARRAY_SIZE(ops); o++) {
				printf(" %s %08x", ops[o].name, r[o].u[lane]);
			}
			printf("\n");
		}
	}
	numInputs = 0;
}

static const unsigned int specials[] = {
	0x00000000, 0x80000000,  // zeros
	0x7f800000, 0xff800000,  // infinities
	0x7fc00000, 0xffc00000,  // quiet NaN
	0x7f800001, 0xff800001,  // signaling NaN
	0x7fffffff, 0x7fc12345,  // NaN payloads
	0x00000001, 0x80000001,  // smallest denormals
	0x007fffff, 0x807fffff,  // largest denormals
	0x00400000, 0x00800000,  // half a denormal range, FLT_MIN
	0x7f7fffff, 0xff7fffff,  // FLT_MAX
	0x3f800000, 0xbf800000,  // 1
	0x40000000, 0x3f000000,  // 2, 0.5
	0x40490fdb, 0x3fc90fdb,  // pi, pi/2
	0x3eaaaaab, 0x3e2aaaab,  // 1/3, 1/6
	0x4b800000, 0x4b800001,  // 2^24, 2^24 + 2: where the fractional part of a revolution runs out
};

static const int exps[] = { -126, -100, -64, -30, -24, -16, -8, -4, -3, -2, -1, 0, 1, 2, 3, 4, 8, 16, 24, 30, 64, 100, 127 };
static const unsigned int mants[] = { 0x000000, 0x000001, 0x400000, 0x7fffff, 0x2aaaaa, 0x5a5a5a };

int main(int argc, char *argv[]) {
	for (int i = 0; i < ARRAY_SIZE(specials); i++) {
		add(specials[i]);
	}
	run("specials");

	for (int s = 0; s < 2; s++) {
		for (int e = 0; e < ARRAY_SIZE(exps); e++) {
			for (int m = 0; m < ARRAY_SIZE(mants); m++) {
				add((s << 31) | ((unsigned int)(exps[e] + 127) << 23) | mants[m]);
			}
		}
	}
	run("sweep");

	// Random bit patterns with the exponent in [-12, 12].
	unsigned int seed = 0x12345678;
	for (int i = 0; i < 64; i++) {
		seed = seed * 1664525u + 1013904223u;
		unsigned int sign = seed & 0x80000000u;
		unsigned int e = 127 - 12 + ((seed >> 23) % 25);
		add(sign | (e << 23) | (seed & 0x7fffff));
	}
	run("random");
	return 0;
}
