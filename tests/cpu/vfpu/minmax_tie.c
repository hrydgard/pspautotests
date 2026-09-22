#include <common.h>
#include "vfpu_common.h"
#include <string.h>
#include <math.h>

// vmin/vmax when the two operands are equal but not identical: -0 and +0. Which one comes out?
// And does it depend on the lane, on the vector size, on what was in the destination register,
// or on the other lanes? Every op here first sets the destination to a marker (or to a chosen
// zero) so that "kept the old value" is visible too.

typedef struct { unsigned int x, y, z, w; } UVec4;

// The register names differ by size: S200 for .s, C200 otherwise.
#define TIE_OP(name, instr, sz, reg) \
	static void __attribute__((noinline)) name(ScePspFVector4 *out, const ScePspFVector4 *a, const ScePspFVector4 *b, const ScePspFVector4 *dprev) { \
		asm volatile ( \
			"lv.q   C100, %1\n" \
			"lv.q   C110, %2\n" \
			"lv.q   C200, %3\n" \
			instr "." sz " " reg "200, " reg "100, " reg "110\n" \
			"sv.q   C200, %0\n" \
			: "+m" (*out) : "m" (*a), "m" (*b), "m" (*dprev) \
		); \
	}

TIE_OP(vmin_s, "vmin", "s", "S")
TIE_OP(vmin_p, "vmin", "p", "C")
TIE_OP(vmin_t, "vmin", "t", "C")
TIE_OP(vmin_q, "vmin", "q", "C")
TIE_OP(vmax_s, "vmax", "s", "S")
TIE_OP(vmax_p, "vmax", "p", "C")
TIE_OP(vmax_t, "vmax", "t", "C")
TIE_OP(vmax_q, "vmax", "q", "C")

// Same register on both sides.
static void __attribute__((noinline)) vmin_q_same(ScePspFVector4 *out, const ScePspFVector4 *a) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vmin.q C200, C100, C100\n"
		"sv.q   C200, %0\n"
		: "+m" (*out) : "m" (*a)
	);
}

// Destination is one of the sources.
static void __attribute__((noinline)) vmin_q_inplace(ScePspFVector4 *out, const ScePspFVector4 *a, const ScePspFVector4 *b) {
	asm volatile (
		"lv.q   C100, %1\n"
		"lv.q   C110, %2\n"
		"vmin.q C100, C100, C110\n"
		"sv.q   C100, %0\n"
		: "+m" (*out) : "m" (*a), "m" (*b)
	);
}

typedef void (*TieFunc)(ScePspFVector4 *, const ScePspFVector4 *, const ScePspFVector4 *, const ScePspFVector4 *);

static const struct { const char *name; TieFunc func; } ops[] = {
	{ "vmin.s", vmin_s }, { "vmin.p", vmin_p }, { "vmin.t", vmin_t }, { "vmin.q", vmin_q },
	{ "vmax.s", vmax_s }, { "vmax.p", vmax_p }, { "vmax.t", vmax_t }, { "vmax.q", vmax_q },
};

static void run(const char *label, TieFunc func, const ScePspFVector4 *a, const ScePspFVector4 *b, const ScePspFVector4 *dprev) {
	ALIGN16 ScePspFVector4 out;
	UVec4 ua, ub, ud, r;
	func(&out, a, b, dprev);
	memcpy(&ua, a, 16); memcpy(&ub, b, 16); memcpy(&ud, dprev, 16); memcpy(&r, &out, 16);
	printf("%s a=%08x,%08x,%08x,%08x b=%08x,%08x,%08x,%08x d=%08x,%08x,%08x,%08x -> %08x,%08x,%08x,%08x\n", label,
		ua.x, ua.y, ua.z, ua.w, ub.x, ub.y, ub.z, ub.w, ud.x, ud.y, ud.z, ud.w, r.x, r.y, r.z, r.w);
}

int main(int argc, char *argv[]) {
	ALIGN16 ScePspFVector4 negZeros = { -0.0f, -0.0f, -0.0f, -0.0f };
	ALIGN16 ScePspFVector4 posZeros = { 0.0f, 0.0f, 0.0f, 0.0f };
	ALIGN16 ScePspFVector4 marker = { 1.0f, 2.0f, 3.0f, 4.0f };
	ALIGN16 ScePspFVector4 mixed1 = { -0.0f, 0.0f, -0.0f, 0.0f };
	ALIGN16 ScePspFVector4 mixed2 = { 0.0f, -0.0f, -0.0f, 0.0f };
	ALIGN16 ScePspFVector4 out;
	UVec4 r;

	printf("-- every size, every lane, all ties, destination = marker --\n");
	for (int o = 0; o < ARRAY_SIZE(ops); o++) {
		run(ops[o].name, ops[o].func, &negZeros, &posZeros, &marker);
		run(ops[o].name, ops[o].func, &posZeros, &negZeros, &marker);
		run(ops[o].name, ops[o].func, &negZeros, &negZeros, &marker);
		run(ops[o].name, ops[o].func, &posZeros, &posZeros, &marker);
	}

	printf("-- does the old destination matter? --\n");
	for (int o = 3; o < ARRAY_SIZE(ops); o += 4) {
		run(ops[o].name, ops[o].func, &negZeros, &posZeros, &negZeros);
		run(ops[o].name, ops[o].func, &negZeros, &posZeros, &posZeros);
		run(ops[o].name, ops[o].func, &posZeros, &negZeros, &negZeros);
		run(ops[o].name, ops[o].func, &posZeros, &negZeros, &posZeros);
	}

	printf("-- mixed lanes --\n");
	for (int o = 3; o < ARRAY_SIZE(ops); o += 4) {
		run(ops[o].name, ops[o].func, &mixed1, &mixed2, &marker);
		run(ops[o].name, ops[o].func, &mixed2, &mixed1, &marker);
	}

	printf("-- the vectors from minmax.c, standalone --\n");
	{
		ALIGN16 ScePspFVector4 a4 = { -1.0f, -3.0f, -0.0f, -1e30f };
		ALIGN16 ScePspFVector4 b4 = { -2.0f, -2.5f, 0.0f, -1e-30f };
		ALIGN16 ScePspFVector4 a5 = { INFINITY, -INFINITY, 0.0f, 5.0f };
		ALIGN16 ScePspFVector4 b5 = { -INFINITY, INFINITY, -0.0f, 5.0f };
		run("vmin.q", vmin_q, &a4, &b4, &marker);
		run("vmax.q", vmax_q, &a4, &b4, &marker);
		run("vmin.q", vmin_q, &a5, &b5, &marker);
		run("vmax.q", vmax_q, &a5, &b5, &marker);
		// And with the zero tie next to non-tie lanes of each sign.
		ALIGN16 ScePspFVector4 a6 = { 1.0f, 0.0f, -1.0f, 0.0f };
		ALIGN16 ScePspFVector4 b6 = { 2.0f, -0.0f, -2.0f, -0.0f };
		run("vmin.q", vmin_q, &a6, &b6, &marker);
		run("vmax.q", vmax_q, &a6, &b6, &marker);
		run("vmin.q", vmin_q, &b6, &a6, &marker);
		run("vmax.q", vmax_q, &b6, &a6, &marker);
	}

	printf("-- same register both sides, and in place --\n");
	vmin_q_same(&out, &mixed1); memcpy(&r, &out, 16);
	printf("vmin.q C100,C100 (%08x,%08x,%08x,%08x) -> %08x,%08x,%08x,%08x\n", 0x80000000u, 0u, 0x80000000u, 0u, r.x, r.y, r.z, r.w);
	vmin_q_inplace(&out, &negZeros, &posZeros); memcpy(&r, &out, 16);
	printf("vmin.q C100,C100,C110 (-0 vs +0) -> %08x,%08x,%08x,%08x\n", r.x, r.y, r.z, r.w);
	vmin_q_inplace(&out, &posZeros, &negZeros); memcpy(&r, &out, 16);
	printf("vmin.q C100,C100,C110 (+0 vs -0) -> %08x,%08x,%08x,%08x\n", r.x, r.y, r.z, r.w);
	return 0;
}
