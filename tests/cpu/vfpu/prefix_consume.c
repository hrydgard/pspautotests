#include <common.h>
#include "vfpu_common.h"
#include <string.h>
#include <math.h>

// Which instructions consume the pending prefixes, and which apply them. For each op under test:
// set all three prefixes (a swizzle+negate on S, a constant on T, a saturate on D), run the op,
// then run a plain vadd.q, and print both results. The vadd shows whether the prefixes were
// still pending (they'd change its result) or eaten. Reading the prefix control registers back
// afterwards shows the same thing from the other side.
//
// The prefixes are chosen so that every one of them is visible if applied:
//   S: [-y, -x, -w, -z]   T: [1, 1/2, 3, 0]   D: [0:1, 0:1, 0:1, 0:1]
// and the vadd's inputs are chosen so that it's visible if any survive.

typedef struct { unsigned int x, y, z, w; } UVec4;

static ALIGN16 const ScePspFVector4 inS = { 1.5f, -2.5f, 3.5f, -4.5f };
static ALIGN16 const ScePspFVector4 inT = { 10.0f, 20.0f, 30.0f, 40.0f };
static ALIGN16 const ScePspFVector4 inA = { 0.25f, 0.5f, 0.75f, 1.25f };
static ALIGN16 const ScePspFVector4 inB = { 100.0f, 200.0f, 300.0f, 400.0f };
ALIGN16 ScePspFVector4 scratch;

struct Result {
	ScePspFVector4 op;     // what the op wrote (C000)
	ScePspFVector4 after;  // what the following vadd.q wrote (C010)
	unsigned int spfx, tpfx, dpfx;  // control registers after the op, before the vadd
};

// The prefix setup and the follow-up are the same for every op; only the middle differs.
#define CONSUME_TEST(name, ops) \
	static void __attribute__((noinline)) name(struct Result *r) { \
		asm volatile ( \
			"lv.q   C100, %1\n" \
			"lv.q   C110, %2\n" \
			"lv.q   C120, %3\n" \
			"lv.q   C130, %4\n" \
			"vmov.q C000, C000[0, 0, 0, 0]\n" \
			"vmov.q C010, C010[0, 0, 0, 0]\n" \
			"vpfxs  -y, -x, -w, -z\n" \
			"vpfxt  1, 1/2, 3, 0\n" \
			"vpfxd  0:1, 0:1, 0:1, 0:1\n" \
			ops \
			"mfvc   $t0, $128\n" \
			"mfvc   $t1, $129\n" \
			"mfvc   $t2, $130\n" \
			"vadd.q C010, C120, C130\n" \
			"sv.q   C000, 0+%0\n" \
			"sv.q   C010, 16+%0\n" \
			"sw     $t0, 32+%0\n" \
			"sw     $t1, 36+%0\n" \
			"sw     $t2, 40+%0\n" \
			: "+m" (*r) : "m" (inS), "m" (inT), "m" (inA), "m" (inB) : "t0", "t1", "t2", "t3", "memory" \
		); \
	}

CONSUME_TEST(t_none,   "")
CONSUME_TEST(t_vadd_q, "vadd.q C000, C100, C110\n")
CONSUME_TEST(t_vadd_s, "vadd.s S000, S100, S110\n")
CONSUME_TEST(t_vadd_p, "vadd.p C000, C100, C110\n")
CONSUME_TEST(t_vsub_q, "vsub.q C000, C100, C110\n")
CONSUME_TEST(t_vmul_q, "vmul.q C000, C100, C110\n")
CONSUME_TEST(t_vdiv_q, "vdiv.q C000, C100, C110\n")
CONSUME_TEST(t_vmin_q, "vmin.q C000, C100, C110\n")
CONSUME_TEST(t_vmax_q, "vmax.q C000, C100, C110\n")
CONSUME_TEST(t_vscl_q, "vscl.q C000, C100, S110\n")
CONSUME_TEST(t_vdot_q, "vdot.q S000, C100, C110\n")
CONSUME_TEST(t_vhdp_q, "vhdp.q S000, C100, C110\n")
CONSUME_TEST(t_vcrs_t, "vcrs.t C000, C100, C110\n")
CONSUME_TEST(t_vcrsp_t, "vcrsp.t C000, C100, C110\n")
CONSUME_TEST(t_vqmul_q, "vqmul.q C000, C100, C110\n")
CONSUME_TEST(t_vmov_q, "vmov.q C000, C100\n")
CONSUME_TEST(t_vneg_q, "vneg.q C000, C100\n")
CONSUME_TEST(t_vabs_q, "vabs.q C000, C100\n")
CONSUME_TEST(t_vsat0_q, "vsat0.q C000, C100\n")
CONSUME_TEST(t_vsat1_q, "vsat1.q C000, C100\n")
CONSUME_TEST(t_vzero_q, "vzero.q C000\n")
CONSUME_TEST(t_vone_q,  "vone.q C000\n")
CONSUME_TEST(t_vidt_q,  "vidt.q C000\n")
CONSUME_TEST(t_vcst_q,  "vcst.q C000, VFPU_PI\n")
CONSUME_TEST(t_vrcp_q,  "vrcp.q C000, C100\n")
CONSUME_TEST(t_vrsq_q,  "vrsq.q C000, C100\n")
CONSUME_TEST(t_vsin_q,  "vsin.q C000, C100\n")
CONSUME_TEST(t_vcos_q,  "vcos.q C000, C100\n")
CONSUME_TEST(t_vexp2_q, "vexp2.q C000, C100\n")
CONSUME_TEST(t_vlog2_q, "vlog2.q C000, C100\n")
CONSUME_TEST(t_vsqrt_q, "vsqrt.q C000, C100\n")
CONSUME_TEST(t_vasin_q, "vasin.q C000, C100\n")
CONSUME_TEST(t_vnrcp_q, "vnrcp.q C000, C100\n")
CONSUME_TEST(t_vnsin_q, "vnsin.q C000, C100\n")
CONSUME_TEST(t_vrexp2_q, "vrexp2.q C000, C100\n")
CONSUME_TEST(t_vsgn_q,  "vsgn.q C000, C100\n")
CONSUME_TEST(t_vocp_q,  "vocp.q C000, C100\n")
CONSUME_TEST(t_vsocp_p, "vsocp.p C000, C100\n")
CONSUME_TEST(t_vbfy1_q, "vbfy1.q C000, C100\n")
CONSUME_TEST(t_vbfy2_q, "vbfy2.q C000, C100\n")
CONSUME_TEST(t_vavg_q,  "vavg.q S000, C100\n")
CONSUME_TEST(t_vfad_q,  "vfad.q S000, C100\n")
CONSUME_TEST(t_vf2iz_q, "vf2iz.q C000, C100, 0\n")
CONSUME_TEST(t_vf2in_q, "vf2in.q C000, C100, 2\n")
CONSUME_TEST(t_vi2f_q,  "vi2f.q C000, C100, 0\n")
CONSUME_TEST(t_vf2h_q,  "vf2h.q C000, C100\n")
CONSUME_TEST(t_vh2f_p,  "vh2f.p C000, C100\n")
CONSUME_TEST(t_vi2c_q,  "vi2c.q S000, C100\n")
CONSUME_TEST(t_vi2s_q,  "vi2s.q C000, C100\n")
CONSUME_TEST(t_vc2i_s,  "vc2i.s C000, S100\n")
CONSUME_TEST(t_vs2i_p,  "vs2i.p C000, C100\n")
CONSUME_TEST(t_vsge_q,  "vsge.q C000, C100, C110\n")
CONSUME_TEST(t_vslt_q,  "vslt.q C000, C100, C110\n")
CONSUME_TEST(t_vcmp_q,  "vcmp.q LT, C100, C110\n")
CONSUME_TEST(t_vcmovt_q, "vcmp.q LT, C100, C110\nvcmovt.q C000, C100, 0\n")
CONSUME_TEST(t_vcmovf_q, "vcmp.q LT, C100, C110\nvcmovf.q C000, C100, 0\n")
CONSUME_TEST(t_vrot_q,  "vrot.q C000, S100, [c, s, s, s]\n")
CONSUME_TEST(t_vmmov_q, "vmmov.q M000, M100\n")
CONSUME_TEST(t_vmzero_q, "vmzero.q M000\n")
CONSUME_TEST(t_vmidt_q, "vmidt.q M000\n")
CONSUME_TEST(t_vmscl_q, "vmscl.q M000, M100, S110\n")
CONSUME_TEST(t_vmmul_q, "vmmul.q M000, M100, M100\n")
CONSUME_TEST(t_vtfm4_q, "vtfm4.q C000, M100, C110\n")
CONSUME_TEST(t_vhtfm4_q, "vhtfm4.q C000, M100, C110\n")
CONSUME_TEST(t_vtfm3_t, "vtfm3.t C000, M100, C110\n")
CONSUME_TEST(t_vwbn_s,  "vwbn.s S000, S100, 10\n")
CONSUME_TEST(t_vsbn_s,  "vsbn.s S000, S100, S110\n")
CONSUME_TEST(t_vsbz_s,  "vsbz.s S000, S100\n")
CONSUME_TEST(t_vlgb_s,  "vlgb.s S000, S100\n")
CONSUME_TEST(t_vi2uc_q, "vi2uc.q S000, C100\n")
CONSUME_TEST(t_vi2us_q, "vi2us.q C000, C100\n")
CONSUME_TEST(t_vuc2i_s, "vuc2ifs.s C000, S100\n")
CONSUME_TEST(t_vus2i_p, "vus2i.p C000, C100\n")
CONSUME_TEST(t_lv_q,    "la $t3, inA\nlv.q   C000, 0($t3)\n")
CONSUME_TEST(t_lv_s,    "la $t3, inA\nlv.s   S000, 0($t3)\n")
CONSUME_TEST(t_sv_q,    "la $t3, scratch\nsv.q   C100, 0($t3)\n")
CONSUME_TEST(t_mtv,     "mtv    $zero, S000\n")
CONSUME_TEST(t_mfv,     "mfv    $t3, S100\n")
CONSUME_TEST(t_mfvc,    "mfvc   $t3, $131\n")
CONSUME_TEST(t_vnop,    "vnop\n")
CONSUME_TEST(t_vsync,   "vsync\n")
CONSUME_TEST(t_vflush,  "vflush\n")
CONSUME_TEST(t_nop,     "nop\n")
CONSUME_TEST(t_addiu,   "addiu $t3, $zero, 1\n")

typedef void (*TestFunc)(struct Result *);
static const struct { const char *name; TestFunc func; } tests[] = {
	{ "(nothing)", t_none },
	{ "vadd.q", t_vadd_q }, { "vadd.s", t_vadd_s }, { "vadd.p", t_vadd_p },
	{ "vsub.q", t_vsub_q }, { "vmul.q", t_vmul_q }, { "vdiv.q", t_vdiv_q },
	{ "vmin.q", t_vmin_q }, { "vmax.q", t_vmax_q }, { "vscl.q", t_vscl_q },
	{ "vdot.q", t_vdot_q }, { "vhdp.q", t_vhdp_q },
	{ "vcrs.t", t_vcrs_t }, { "vcrsp.t", t_vcrsp_t }, { "vqmul.q", t_vqmul_q },
	{ "vmov.q", t_vmov_q }, { "vneg.q", t_vneg_q }, { "vabs.q", t_vabs_q },
	{ "vsat0.q", t_vsat0_q }, { "vsat1.q", t_vsat1_q },
	{ "vzero.q", t_vzero_q }, { "vone.q", t_vone_q }, { "vidt.q", t_vidt_q }, { "vcst.q", t_vcst_q },
	{ "vrcp.q", t_vrcp_q }, { "vrsq.q", t_vrsq_q }, { "vsin.q", t_vsin_q }, { "vcos.q", t_vcos_q },
	{ "vexp2.q", t_vexp2_q }, { "vlog2.q", t_vlog2_q }, { "vsqrt.q", t_vsqrt_q }, { "vasin.q", t_vasin_q },
	{ "vnrcp.q", t_vnrcp_q }, { "vnsin.q", t_vnsin_q }, { "vrexp2.q", t_vrexp2_q },
	{ "vsgn.q", t_vsgn_q }, { "vocp.q", t_vocp_q }, { "vsocp.p", t_vsocp_p },
	{ "vbfy1.q", t_vbfy1_q }, { "vbfy2.q", t_vbfy2_q }, { "vavg.q", t_vavg_q }, { "vfad.q", t_vfad_q },
	{ "vf2iz.q", t_vf2iz_q }, { "vf2in.q 2", t_vf2in_q }, { "vi2f.q", t_vi2f_q },
	{ "vf2h.q", t_vf2h_q }, { "vh2f.p", t_vh2f_p },
	{ "vi2c.q", t_vi2c_q }, { "vi2s.q", t_vi2s_q }, { "vc2i.s", t_vc2i_s }, { "vs2i.p", t_vs2i_p },
	{ "vi2uc.q", t_vi2uc_q }, { "vi2us.q", t_vi2us_q }, { "vuc2ifs.s", t_vuc2i_s }, { "vus2i.p", t_vus2i_p },
	{ "vsge.q", t_vsge_q }, { "vslt.q", t_vslt_q },
	{ "vcmp.q", t_vcmp_q }, { "vcmp+vcmovt.q", t_vcmovt_q }, { "vcmp+vcmovf.q", t_vcmovf_q },
	{ "vrot.q", t_vrot_q },
	{ "vmmov.q", t_vmmov_q }, { "vmzero.q", t_vmzero_q }, { "vmidt.q", t_vmidt_q },
	{ "vmscl.q", t_vmscl_q }, { "vmmul.q", t_vmmul_q },
	{ "vtfm4.q", t_vtfm4_q }, { "vhtfm4.q", t_vhtfm4_q }, { "vtfm3.t", t_vtfm3_t },
	{ "vwbn.s", t_vwbn_s }, { "vsbn.s", t_vsbn_s }, { "vsbz.s", t_vsbz_s }, { "vlgb.s", t_vlgb_s },
	{ "lv.q", t_lv_q }, { "lv.s", t_lv_s }, { "sv.q", t_sv_q },
	{ "mtv", t_mtv }, { "mfv", t_mfv }, { "mfvc", t_mfvc },
	{ "vnop", t_vnop }, { "vsync", t_vsync }, { "vflush", t_vflush },
	{ "nop", t_nop }, { "addiu", t_addiu },
};

static void printVec(const char *label, const ScePspFVector4 *v) {
	UVec4 u;
	memcpy(&u, v, 16);
	printf("%s%08x,%08x,%08x,%08x", label, u.x, u.y, u.z, u.w);
}

int main(int argc, char *argv[]) {
	ALIGN16 struct Result r;
	for (int i = 0; i < ARRAY_SIZE(tests); i++) {
		memset(&r, 0xCC, sizeof(r));
		tests[i].func(&r);
		printf("%-14s ", tests[i].name);
		printVec("op=", &r.op);
		printVec(" then=", &r.after);
		printf(" pfx=%08x,%08x,%08x\n", r.spfx, r.tpfx, r.dpfx);
	}
	return 0;
}
