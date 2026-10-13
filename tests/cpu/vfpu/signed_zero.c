#include <common.h>
#include "vfpu_common.h"

// The sign of a zero that comes out of a sum: whether the dot products, the matrix ops and the
// horizontal adds start from +0 (so that -0 products sum to +0) or from the first product (so
// that they sum to -0), at every size. One lane of each has a +0 product, the rest -0, to see
// whether the order matters too. vfad, vavg and vadd read only t, which holds the zeros.

typedef struct { unsigned int v[4]; } __attribute__((aligned(16))) UVec4;
typedef struct { unsigned int v[16]; } __attribute__((aligned(16))) UMat4;

#define NZ 0x80000000
#define PZ 0x00000000
#define ONE 0x3f800000
#define NONE 0xbf800000

// The vector ops read C100 and C200, and write C000 (cleared to deadbeef first).
#define VECOP(name, instr) \
	static void NOINLINE name(UVec4 *out, const UVec4 *s, const UVec4 *t) { \
		asm volatile ( \
			"lv.q C000, %0\n" \
			"lv.q C100, %1\n" \
			"lv.q C200, %2\n" \
			instr "\n" \
			"sv.q C000, %0\n" \
			: "+m" (*out) : "m" (*s), "m" (*t) : "memory" \
		); \
	}

VECOP(vdot_p, "vdot.p S000, C100, C200")
VECOP(vdot_t, "vdot.t S000, C100, C200")
VECOP(vdot_q, "vdot.q S000, C100, C200")
VECOP(vhdp_p, "vhdp.p S000, C100, C200")
VECOP(vhdp_t, "vhdp.t S000, C100, C200")
VECOP(vhdp_q, "vhdp.q S000, C100, C200")
VECOP(vfad_p, "vfad.p S000, C200")
VECOP(vfad_t, "vfad.t S000, C200")
VECOP(vfad_q, "vfad.q S000, C200")
VECOP(vavg_p, "vavg.p S000, C200")
VECOP(vavg_t, "vavg.t S000, C200")
VECOP(vavg_q, "vavg.q S000, C200")
VECOP(vdet_p, "vdet.p S000, C100, C200")
VECOP(vcrs_t, "vcrs.t C000, C100, C200")
VECOP(vcrsp_t, "vcrsp.t C000, C100, C200")
VECOP(vqmul_q, "vqmul.q C000, C100, C200")
VECOP(vadd_q, "vadd.q C000, C200, C200")
VECOP(vsub_q, "vsub.q C000, C100, C200")

// The matrix ops read M100 (and M200 or C200), and write M000 or C000.
#define MATOP(name, instr) \
	static void NOINLINE name(UMat4 *out, const UMat4 *m, const UMat4 *t) { \
		asm volatile ( \
			"lv.q C000, 0x00+%0\n" \
			"lv.q C010, 0x10+%0\n" \
			"lv.q C020, 0x20+%0\n" \
			"lv.q C030, 0x30+%0\n" \
			"lv.q C100, 0x00+%1\n" \
			"lv.q C110, 0x10+%1\n" \
			"lv.q C120, 0x20+%1\n" \
			"lv.q C130, 0x30+%1\n" \
			"lv.q C200, 0x00+%2\n" \
			"lv.q C210, 0x10+%2\n" \
			"lv.q C220, 0x20+%2\n" \
			"lv.q C230, 0x30+%2\n" \
			instr "\n" \
			"sv.q C000, 0x00+%0\n" \
			"sv.q C010, 0x10+%0\n" \
			"sv.q C020, 0x20+%0\n" \
			"sv.q C030, 0x30+%0\n" \
			: "+m" (*out) : "m" (*m), "m" (*t) : "memory" \
		); \
	}

MATOP(vmmul_p, "vmmul.p M000, M100, M200")
MATOP(vmmul_t, "vmmul.t M000, M100, M200")
MATOP(vmmul_q, "vmmul.q M000, M100, M200")
MATOP(vtfm2_p, "vtfm2.p C000, M100, C200")
MATOP(vtfm3_t, "vtfm3.t C000, M100, C200")
MATOP(vtfm4_q, "vtfm4.q C000, M100, C200")
MATOP(vhtfm2_p, "vhtfm2.p C000, M100, C200")
MATOP(vhtfm3_t, "vhtfm3.t C000, M100, C200")
MATOP(vhtfm4_q, "vhtfm4.q C000, M100, C200")

typedef void (*VecFunc)(UVec4 *, const UVec4 *, const UVec4 *);
typedef void (*MatFunc)(UMat4 *, const UMat4 *, const UMat4 *);

static const struct {
	const char *name;
	VecFunc func;
} vecOps[] = {
	{ "vdot.p ", vdot_p }, { "vdot.t ", vdot_t }, { "vdot.q ", vdot_q },
	{ "vhdp.p ", vhdp_p }, { "vhdp.t ", vhdp_t }, { "vhdp.q ", vhdp_q },
	{ "vfad.p ", vfad_p }, { "vfad.t ", vfad_t }, { "vfad.q ", vfad_q },
	{ "vavg.p ", vavg_p }, { "vavg.t ", vavg_t }, { "vavg.q ", vavg_q },
	{ "vdet.p ", vdet_p },
	{ "vcrs.t ", vcrs_t }, { "vcrsp.t", vcrsp_t }, { "vqmul.q", vqmul_q },
	{ "vadd.q ", vadd_q }, { "vsub.q ", vsub_q },
};

static const struct {
	const char *name;
	UVec4 s, t;
} vecInputs[] = {
	// Every product -0.
	{ "all -0     ", {{ ONE, ONE, ONE, ONE }}, {{ NZ, NZ, NZ, NZ }} },
	{ "-1 * +0    ", {{ NONE, NONE, NONE, NONE }}, {{ PZ, PZ, PZ, PZ }} },
	// Every input -0, so products are +0 and the vfad/vavg inputs are -0.
	{ "-0 * -0    ", {{ NZ, NZ, NZ, NZ }}, {{ NZ, NZ, NZ, NZ }} },
	// One +0 product, in each position.
	{ "+0 in x    ", {{ ONE, ONE, ONE, ONE }}, {{ PZ, NZ, NZ, NZ }} },
	{ "+0 in y    ", {{ ONE, ONE, ONE, ONE }}, {{ NZ, PZ, NZ, NZ }} },
	{ "+0 in z    ", {{ ONE, ONE, ONE, ONE }}, {{ NZ, NZ, PZ, NZ }} },
	{ "+0 in w    ", {{ ONE, ONE, ONE, ONE }}, {{ NZ, NZ, NZ, PZ }} },
	// Products that cancel to zero: 1 + -1.
	{ "1 - 1      ", {{ ONE, ONE, ONE, ONE }}, {{ ONE, NONE, ONE, NONE }} },
};

static const struct {
	const char *name;
	UMat4 m, t;
} matInputs[] = {
	{ "ones * -0  ",
		{{ ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE }},
		{{ NZ, NZ, NZ, NZ, NZ, NZ, NZ, NZ, NZ, NZ, NZ, NZ, NZ, NZ, NZ, NZ }} },
	{ "-0 * ones  ",
		{{ NZ, NZ, NZ, NZ, NZ, NZ, NZ, NZ, NZ, NZ, NZ, NZ, NZ, NZ, NZ, NZ }},
		{{ ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE }} },
	// A +0 on the diagonal of each.
	{ "mixed      ",
		{{ PZ, NZ, NZ, NZ, NZ, PZ, NZ, NZ, NZ, NZ, PZ, NZ, NZ, NZ, NZ, PZ }},
		{{ ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE }} },
};

static const struct {
	const char *name;
	MatFunc func;
} matOps[] = {
	{ "vmmul.p  ", vmmul_p }, { "vmmul.t  ", vmmul_t }, { "vmmul.q  ", vmmul_q },
	{ "vtfm2.p  ", vtfm2_p }, { "vtfm3.t  ", vtfm3_t }, { "vtfm4.q  ", vtfm4_q },
	{ "vhtfm2.p ", vhtfm2_p }, { "vhtfm3.t ", vhtfm3_t }, { "vhtfm4.q ", vhtfm4_q },
};

int main(int argc, char *argv[]) {
	for (int i = 0; i < ARRAY_SIZE(vecInputs); i++) {
		printf("== %s ==\n", vecInputs[i].name);
		for (int j = 0; j < ARRAY_SIZE(vecOps); j++) {
			UVec4 out = {{ 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef }};
			vecOps[j].func(&out, &vecInputs[i].s, &vecInputs[i].t);
			printf("%s: %08x %08x %08x %08x\n", vecOps[j].name, out.v[0], out.v[1], out.v[2], out.v[3]);
		}
	}
	for (int i = 0; i < ARRAY_SIZE(matInputs); i++) {
		printf("== %s ==\n", matInputs[i].name);
		for (int j = 0; j < ARRAY_SIZE(matOps); j++) {
			UMat4 out;
			for (int k = 0; k < 16; k++) {
				out.v[k] = 0xdeadbeef;
			}
			matOps[j].func(&out, &matInputs[i].m, &matInputs[i].t);
			printf("%s:", matOps[j].name);
			for (int k = 0; k < 16; k++) {
				printf(" %08x", out.v[k]);
			}
			printf("\n");
		}
	}
	return 0;
}
