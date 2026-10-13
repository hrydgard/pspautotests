#include <common.h>
#include "vfpu_common.h"

// Which NaN each VFPU op makes from operands that aren't NaN (inf - inf, 0 * inf, 0 / 0,
// sqrt(-1) and the like), and which NaN comes out when an operand already is one: the payload
// and sign kept, and which operand wins. x86 and ARM hosts make different NaNs, so an emulator
// has to pick one; this is the one to pick.

typedef struct { unsigned int v[4]; } __attribute__((aligned(16))) UVec4;
typedef struct { unsigned int v[16]; } __attribute__((aligned(16))) UMat4;

#define INF  0x7f800000
#define NINF 0xff800000
#define PZ   0x00000000
#define NZ   0x80000000
#define ONE  0x3f800000
#define NONE 0xbf800000
#define TWO  0x40000000

// Reads C100 and C200, writes C000 (preset to deadbeef).
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

VECOP(vadd_q, "vadd.q C000, C100, C200")
VECOP(vsub_q, "vsub.q C000, C100, C200")
VECOP(vmul_q, "vmul.q C000, C100, C200")
VECOP(vdiv_q, "vdiv.q C000, C100, C200")
VECOP(vscl_q, "vscl.q C000, C100, S200")
VECOP(vdot_q, "vdot.q S000, C100, C200")
VECOP(vhdp_q, "vhdp.q S000, C100, C200")
VECOP(vcrs_t, "vcrs.t C000, C100, C200")
VECOP(vcrsp_t, "vcrsp.t C000, C100, C200")
VECOP(vqmul_q, "vqmul.q C000, C100, C200")
VECOP(vdet_p, "vdet.p S000, C100, C200")
VECOP(vfad_q, "vfad.q S000, C100")
VECOP(vavg_q, "vavg.q S000, C100")
VECOP(vsqrt_q, "vsqrt.q C000, C100")
VECOP(vrsq_q, "vrsq.q C000, C100")
VECOP(vrcp_q, "vrcp.q C000, C100")
VECOP(vnrcp_q, "vnrcp.q C000, C100")
VECOP(vlog2_q, "vlog2.q C000, C100")
VECOP(vexp2_q, "vexp2.q C000, C100")
VECOP(vrexp2_q, "vrexp2.q C000, C100")
VECOP(vsin_q, "vsin.q C000, C100")
VECOP(vcos_q, "vcos.q C000, C100")
VECOP(vnsin_q, "vnsin.q C000, C100")
VECOP(vasin_q, "vasin.q C000, C100")
VECOP(vsat0_q, "vsat0.q C000, C100")
VECOP(vsat1_q, "vsat1.q C000, C100")
VECOP(vabs_q, "vabs.q C000, C100")
VECOP(vneg_q, "vneg.q C000, C100")

// Reads M100 and M200 (or C200), writes M000.
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

MATOP(vmmul_q, "vmmul.q M000, M100, M200")
MATOP(vtfm4_q, "vtfm4.q C000, M100, C200")
MATOP(vhtfm4_q, "vhtfm4.q C000, M100, C200")

typedef void (*VecFunc)(UVec4 *, const UVec4 *, const UVec4 *);
typedef void (*MatFunc)(UMat4 *, const UMat4 *, const UMat4 *);

static const struct {
	const char *name;
	VecFunc func;
	UVec4 s, t;
} generated[] = {
	{ "vadd  inf + -inf     ", vadd_q, {{ INF, NINF, INF, NINF }}, {{ NINF, INF, NINF, INF }} },
	{ "vsub  inf - inf      ", vsub_q, {{ INF, NINF, INF, NINF }}, {{ INF, NINF, INF, NINF }} },
	{ "vmul  0 * inf        ", vmul_q, {{ PZ, NZ, INF, NINF }}, {{ INF, INF, PZ, PZ }} },
	{ "vdiv  0/0, inf/inf   ", vdiv_q, {{ PZ, NZ, INF, NINF }}, {{ PZ, PZ, INF, INF }} },
	{ "vscl  x * inf        ", vscl_q, {{ PZ, NZ, ONE, NONE }}, {{ INF, PZ, PZ, PZ }} },
	{ "vdot  inf * 0        ", vdot_q, {{ INF, PZ, PZ, PZ }}, {{ PZ, PZ, PZ, PZ }} },
	{ "vdot  inf - inf      ", vdot_q, {{ INF, INF, PZ, PZ }}, {{ ONE, NONE, PZ, PZ }} },
	{ "vhdp  inf * 0        ", vhdp_q, {{ INF, PZ, PZ, PZ }}, {{ PZ, PZ, PZ, PZ }} },
	{ "vhdp  inf - inf      ", vhdp_q, {{ INF, PZ, PZ, PZ }}, {{ ONE, PZ, PZ, NINF }} },
	{ "vcrs  inf * 0        ", vcrs_t, {{ INF, INF, INF, PZ }}, {{ PZ, PZ, PZ, PZ }} },
	{ "vcrsp inf * 0        ", vcrsp_t, {{ INF, INF, INF, PZ }}, {{ PZ, PZ, PZ, PZ }} },
	{ "vqmul inf * 0        ", vqmul_q, {{ INF, INF, INF, INF }}, {{ PZ, PZ, PZ, PZ }} },
	{ "vdet  inf * 0        ", vdet_p, {{ INF, PZ, PZ, PZ }}, {{ PZ, PZ, PZ, PZ }} },
	{ "vdet  inf - inf      ", vdet_p, {{ INF, INF, PZ, PZ }}, {{ INF, INF, PZ, PZ }} },
	{ "vfad  inf + -inf     ", vfad_q, {{ INF, NINF, PZ, PZ }}, {{ PZ, PZ, PZ, PZ }} },
	{ "vavg  inf + -inf     ", vavg_q, {{ INF, NINF, PZ, PZ }}, {{ PZ, PZ, PZ, PZ }} },
	{ "vsqrt -1 -inf -min -0", vsqrt_q, {{ NONE, NINF, 0x80800000, NZ }}, {{ PZ, PZ, PZ, PZ }} },
	{ "vrsq  -1 -inf -0 0   ", vrsq_q, {{ NONE, NINF, NZ, PZ }}, {{ PZ, PZ, PZ, PZ }} },
	{ "vrcp  0 -0 inf -inf  ", vrcp_q, {{ PZ, NZ, INF, NINF }}, {{ PZ, PZ, PZ, PZ }} },
	{ "vnrcp 0 -0 inf -inf  ", vnrcp_q, {{ PZ, NZ, INF, NINF }}, {{ PZ, PZ, PZ, PZ }} },
	{ "vlog2 -1 -inf 0 -0   ", vlog2_q, {{ NONE, NINF, PZ, NZ }}, {{ PZ, PZ, PZ, PZ }} },
	{ "vexp2 inf -inf ...   ", vexp2_q, {{ INF, NINF, 0x43000000, 0xc3200000 }}, {{ PZ, PZ, PZ, PZ }} },
	{ "vrexp2 inf -inf 0 -0 ", vrexp2_q, {{ INF, NINF, PZ, NZ }}, {{ PZ, PZ, PZ, PZ }} },
	{ "vsin  inf -inf ...   ", vsin_q, {{ INF, NINF, 0x7f7fffff, 0x4b800000 }}, {{ PZ, PZ, PZ, PZ }} },
	{ "vcos  inf -inf ...   ", vcos_q, {{ INF, NINF, 0x7f7fffff, 0x4b800000 }}, {{ PZ, PZ, PZ, PZ }} },
	{ "vnsin inf -inf ...   ", vnsin_q, {{ INF, NINF, 0x7f7fffff, 0x4b800000 }}, {{ PZ, PZ, PZ, PZ }} },
	{ "vasin 2 -2 inf -inf  ", vasin_q, {{ TWO, 0xc0000000, INF, NINF }}, {{ PZ, PZ, PZ, PZ }} },
};

// NaN operands: a signaling NaN, a quiet one with a payload, the default ones of both signs.
#define SNAN  0x7f800001
#define NSNAN 0xff812345
#define QNAN  0x7fc00000
#define NQNAN 0xffc00000
#define QPAY  0x7fc54321
#define NQPAY 0xffe12345

static const struct {
	const char *name;
	UVec4 s, t;
} nanInputs[] = {
	{ "nan, 1     ", {{ SNAN, NSNAN, QPAY, NQPAY }}, {{ ONE, ONE, ONE, ONE }} },
	{ "1, nan     ", {{ ONE, ONE, ONE, ONE }}, {{ SNAN, NSNAN, QPAY, NQPAY }} },
	{ "nan, nan   ", {{ SNAN, QPAY, NQPAY, QNAN }}, {{ QPAY, SNAN, QNAN, NQPAY }} },
	{ "nan, inf/0 ", {{ NSNAN, NQPAY, NQNAN, QPAY }}, {{ INF, NINF, PZ, NZ }} },
};

static const struct {
	const char *name;
	VecFunc func;
} nanOps[] = {
	{ "vadd ", vadd_q }, { "vsub ", vsub_q }, { "vmul ", vmul_q }, { "vdiv ", vdiv_q },
	{ "vscl ", vscl_q }, { "vdot ", vdot_q }, { "vhdp ", vhdp_q }, { "vfad ", vfad_q },
	{ "vsqrt", vsqrt_q }, { "vrcp ", vrcp_q }, { "vrsq ", vrsq_q }, { "vlog2", vlog2_q },
	{ "vexp2", vexp2_q }, { "vsin ", vsin_q }, { "vasin", vasin_q },
	{ "vsat0", vsat0_q }, { "vsat1", vsat1_q }, { "vabs ", vabs_q }, { "vneg ", vneg_q },
};

static const struct {
	const char *name;
	MatFunc func;
	UMat4 m, t;
} matCases[] = {
	{ "vmmul  inf * 0  ", vmmul_q,
		{{ INF, PZ, PZ, PZ, PZ, INF, PZ, PZ, PZ, PZ, INF, PZ, PZ, PZ, PZ, INF }},
		{{ PZ, ONE, ONE, ONE, ONE, PZ, ONE, ONE, ONE, ONE, PZ, ONE, ONE, ONE, ONE, PZ }} },
	{ "vmmul  inf - inf", vmmul_q,
		{{ INF, INF, INF, INF, INF, INF, INF, INF, INF, INF, INF, INF, INF, INF, INF, INF }},
		{{ ONE, NONE, ONE, NONE, ONE, NONE, ONE, NONE, ONE, NONE, ONE, NONE, ONE, NONE, ONE, NONE }} },
	{ "vtfm4  inf * 0  ", vtfm4_q,
		{{ INF, INF, INF, INF, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ }},
		{{ PZ, ONE, ONE, ONE, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ }} },
	{ "vhtfm4 inf * 0  ", vhtfm4_q,
		{{ INF, INF, INF, INF, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ }},
		{{ PZ, ONE, ONE, ONE, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ, PZ }} },
};

static void printVec(const char *name, const UVec4 *v) {
	printf("%s: %08x %08x %08x %08x\n", name, v->v[0], v->v[1], v->v[2], v->v[3]);
}

int main(int argc, char *argv[]) {
	printf("-- generated --\n");
	for (int i = 0; i < ARRAY_SIZE(generated); i++) {
		UVec4 out = {{ 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef }};
		generated[i].func(&out, &generated[i].s, &generated[i].t);
		printVec(generated[i].name, &out);
	}
	for (int i = 0; i < ARRAY_SIZE(matCases); i++) {
		UMat4 out;
		for (int k = 0; k < 16; k++) {
			out.v[k] = 0xdeadbeef;
		}
		matCases[i].func(&out, &matCases[i].m, &matCases[i].t);
		printf("%s:", matCases[i].name);
		for (int k = 0; k < 16; k++) {
			printf(" %08x", out.v[k]);
		}
		printf("\n");
	}

	for (int i = 0; i < ARRAY_SIZE(nanInputs); i++) {
		printf("-- %s --\n", nanInputs[i].name);
		for (int j = 0; j < ARRAY_SIZE(nanOps); j++) {
			UVec4 out = {{ 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef }};
			nanOps[j].func(&out, &nanInputs[i].s, &nanInputs[i].t);
			printVec(nanOps[j].name, &out);
		}
	}
	return 0;
}
