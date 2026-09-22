#include <common.h>
#include "vfpu_common.h"
#include <string.h>
#include <math.h>

// What the destination prefix's saturation does to values that don't saturate in the obvious
// way: NaN, the infinities, the zeros, and denormals. First on vadd, where every lane gets it,
// then on the single-lane ops (vrcp, vsin and friends) where the prefixes only reach the last
// lane, with the S prefix pointing that lane at a real lane, at an out-of-range lane, and at a
// constant - and with the T prefix set to different constants, to see whether it matters to
// ops that have no T operand.

typedef struct { unsigned int x, y, z, w; } UVec4;

static ALIGN16 const ScePspFVector4 specials = { NAN, -NAN, INFINITY, -INFINITY };
static ALIGN16 const ScePspFVector4 zeros = { 0.0f, -0.0f, 1e-40f, -1e-40f };
static ALIGN16 const ScePspFVector4 edges = { 1.0f, -1.0f, 1.0000001f, -1.0000001f };
static ALIGN16 const ScePspFVector4 plain = { 1.5f, -2.5f, 3.5f, -4.5f };
static ALIGN16 const ScePspFVector4 zero4 = { 0.0f, 0.0f, 0.0f, 0.0f };

#define VADD_SAT(name, dpfx) \
	static void __attribute__((noinline)) name(ScePspFVector4 *out, const ScePspFVector4 *a) { \
		asm volatile ( \
			"lv.q   C100, %1\n" \
			"lv.q   C110, %2\n" \
			"vpfxd  " dpfx "\n" \
			"vadd.q C000, C100, C110\n" \
			"sv.q   C000, %0\n" \
			: "+m" (*out) : "m" (*a), "m" (zero4) : "memory" \
		); \
	}

VADD_SAT(vadd_sat01, "0:1, 0:1, 0:1, 0:1")
VADD_SAT(vadd_sat11, "-1:1, -1:1, -1:1, -1:1")

// vsat0/vsat1 for comparison, they saturate without a prefix.
static void __attribute__((noinline)) vsat0_q(ScePspFVector4 *out, const ScePspFVector4 *a) {
	asm volatile ("lv.q C100, %1\nvsat0.q C000, C100\nsv.q C000, %0\n" : "+m" (*out) : "m" (*a) : "memory");
}
static void __attribute__((noinline)) vsat1_q(ScePspFVector4 *out, const ScePspFVector4 *a) {
	asm volatile ("lv.q C100, %1\nvsat1.q C000, C100\nsv.q C000, %0\n" : "+m" (*out) : "m" (*a) : "memory");
}

// A single-lane op on a quad, with chosen S, T and D prefixes.
#define VV_PFX(name, instr, spfx, tpfx, dpfx) \
	static void __attribute__((noinline)) name(ScePspFVector4 *out, const ScePspFVector4 *a) { \
		asm volatile ( \
			"lv.q   C100, %1\n" \
			"vmov.q C000, C000[0, 0, 0, 0]\n" \
			spfx tpfx dpfx \
			instr ".q C000, C100\n" \
			"sv.q   C000, %0\n" \
			: "+m" (*out) : "m" (*a) : "memory" \
		); \
	}

#define S_NONE ""
#define S_LASTW "vpfxs x, y, z, w\n"
#define S_LASTX "vpfxs x, y, z, x\n"
#define S_LASTNEGZ "vpfxs x, y, z, -z\n"
#define S_LASTABSY "vpfxs x, y, z, |y|\n"
#define S_LASTC0 "vpfxs x, y, z, 0\n"
#define S_LASTC1 "vpfxs x, y, z, 1\n"
#define S_LASTCNEG1 "vpfxs x, y, z, -1\n"
#define S_LASTC2 "vpfxs x, y, z, 2\n"
#define S_SWIZALL "vpfxs -y, -x, -w, -z\n"
#define T_NONE ""
#define T_ZERO "vpfxt 0, 0, 0, 0\n"
#define T_ONE "vpfxt 1, 1, 1, 1\n"
#define T_MIXED "vpfxt 1, 1/2, 3, 0\n"
#define T_SWIZ "vpfxt w, z, y, x\n"
#define D_NONE ""
#define D_SAT01 "vpfxd 0:1, 0:1, 0:1, 0:1\n"
#define D_SAT11 "vpfxd -1:1, -1:1, -1:1, -1:1\n"
#define D_MASKW "vpfxd ,,,m\n"
#define D_SATX "vpfxd 0:1,,,\n"

typedef void (*OpFunc)(ScePspFVector4 *, const ScePspFVector4 *);

#define OP_SET(op, instr) \
	VV_PFX(op##_plain,    instr, S_NONE, T_NONE, D_NONE) \
	VV_PFX(op##_lastw,    instr, S_LASTW, T_NONE, D_NONE) \
	VV_PFX(op##_lastx,    instr, S_LASTX, T_NONE, D_NONE) \
	VV_PFX(op##_lastnegz, instr, S_LASTNEGZ, T_NONE, D_NONE) \
	VV_PFX(op##_lastabsy, instr, S_LASTABSY, T_NONE, D_NONE) \
	VV_PFX(op##_lastc0,   instr, S_LASTC0, T_NONE, D_NONE) \
	VV_PFX(op##_lastc1,   instr, S_LASTC1, T_NONE, D_NONE) \
	VV_PFX(op##_lastcneg1, instr, S_LASTCNEG1, T_NONE, D_NONE) \
	VV_PFX(op##_lastc2,   instr, S_LASTC2, T_NONE, D_NONE) \
	VV_PFX(op##_swizall,  instr, S_SWIZALL, T_NONE, D_NONE) \
	VV_PFX(op##_tzero,    instr, S_NONE, T_ZERO, D_NONE) \
	VV_PFX(op##_tone,     instr, S_NONE, T_ONE, D_NONE) \
	VV_PFX(op##_tmixed,   instr, S_NONE, T_MIXED, D_NONE) \
	VV_PFX(op##_tswiz,    instr, S_NONE, T_SWIZ, D_NONE) \
	VV_PFX(op##_sat01,    instr, S_NONE, T_NONE, D_SAT01) \
	VV_PFX(op##_sat11,    instr, S_NONE, T_NONE, D_SAT11) \
	VV_PFX(op##_maskw,    instr, S_NONE, T_NONE, D_MASKW) \
	VV_PFX(op##_satx,     instr, S_NONE, T_NONE, D_SATX) \
	VV_PFX(op##_all,      instr, S_SWIZALL, T_MIXED, D_SAT01) \
	static const struct { const char *name; OpFunc func; } op##_set[] = { \
		{ "plain", op##_plain }, { "s[.,.,.,w]", op##_lastw }, { "s[.,.,.,x]", op##_lastx }, \
		{ "s[.,.,.,-z]", op##_lastnegz }, { "s[.,.,.,|y|]", op##_lastabsy }, \
		{ "s[.,.,.,0]", op##_lastc0 }, { "s[.,.,.,1]", op##_lastc1 }, { "s[.,.,.,-1]", op##_lastcneg1 }, { "s[.,.,.,2]", op##_lastc2 }, \
		{ "s[-y,-x,-w,-z]", op##_swizall }, \
		{ "t[0,0,0,0]", op##_tzero }, { "t[1,1,1,1]", op##_tone }, { "t[1,1/2,3,0]", op##_tmixed }, { "t[w,z,y,x]", op##_tswiz }, \
		{ "d[0:1 x4]", op##_sat01 }, { "d[-1:1 x4]", op##_sat11 }, { "d[.,.,.,m]", op##_maskw }, { "d[0:1,.,.,.]", op##_satx }, \
		{ "s[-y,-x,-w,-z] t[1,1/2,3,0] d[0:1]", op##_all }, \
	};

OP_SET(vrcp, "vrcp")
OP_SET(vsin, "vsin")
OP_SET(vsqrt, "vsqrt")
OP_SET(vlog2, "vlog2")
OP_SET(vexp2, "vexp2")

// vdiv is the two-operand sibling of these.
#define VDIV_PFX(name, spfx, tpfx, dpfx) \
	static void __attribute__((noinline)) name(ScePspFVector4 *out, const ScePspFVector4 *a) { \
		asm volatile ( \
			"lv.q   C100, %1\n" \
			"lv.q   C110, %2\n" \
			"vmov.q C000, C000[0, 0, 0, 0]\n" \
			spfx tpfx dpfx \
			"vdiv.q C000, C100, C110\n" \
			"sv.q   C000, %0\n" \
			: "+m" (*out) : "m" (*a), "m" (plain) : "memory" \
		); \
	}
VDIV_PFX(vdiv_plain, S_NONE, T_NONE, D_NONE)
VDIV_PFX(vdiv_lastx, S_LASTX, T_NONE, D_NONE)
VDIV_PFX(vdiv_lastc2, S_LASTC2, T_NONE, D_NONE)
VDIV_PFX(vdiv_swizall, S_SWIZALL, T_NONE, D_NONE)
VDIV_PFX(vdiv_tzero, S_NONE, T_ZERO, D_NONE)
VDIV_PFX(vdiv_tone, S_NONE, T_ONE, D_NONE)
VDIV_PFX(vdiv_tmixed, S_NONE, T_MIXED, D_NONE)
VDIV_PFX(vdiv_tswiz, S_NONE, T_SWIZ, D_NONE)
VDIV_PFX(vdiv_sat01, S_NONE, T_NONE, D_SAT01)
VDIV_PFX(vdiv_maskw, S_NONE, T_NONE, D_MASKW)
VDIV_PFX(vdiv_all, S_SWIZALL, T_MIXED, D_SAT01)
static const struct { const char *name; OpFunc func; } vdiv_set[] = {
	{ "plain", vdiv_plain }, { "s[.,.,.,x]", vdiv_lastx }, { "s[.,.,.,2]", vdiv_lastc2 }, { "s[-y,-x,-w,-z]", vdiv_swizall },
	{ "t[0,0,0,0]", vdiv_tzero }, { "t[1,1,1,1]", vdiv_tone }, { "t[1,1/2,3,0]", vdiv_tmixed }, { "t[w,z,y,x]", vdiv_tswiz },
	{ "d[0:1 x4]", vdiv_sat01 }, { "d[.,.,.,m]", vdiv_maskw }, { "s[-y,-x,-w,-z] t[1,1/2,3,0] d[0:1]", vdiv_all },
};

// The same out-of-range swizzles, but with the destination holding a marker first, to tell a
// lane written with zero from a lane not written at all.
static ALIGN16 const ScePspFVector4 marker = { 42.0f, 43.0f, 44.0f, 45.0f };

#define MARKED(name, sizeq, reg, instr, spfx) \
	static void __attribute__((noinline)) name(ScePspFVector4 *out) { \
		asm volatile ( \
			"lv.q   C100, %1\n" \
			"lv.q   C110, %2\n" \
			"lv.q   C000, %3\n" \
			"vpfxs  " spfx "\n" \
			instr "." sizeq " " reg "000, " reg "100" \
			"\nsv.q   C000, %0\n" \
			: "+m" (*out) : "m" (plain), "m" (plain), "m" (marker) : "memory" \
		); \
	}
#define MARKED2(name, sizeq, reg, instr, spfx) \
	static void __attribute__((noinline)) name(ScePspFVector4 *out) { \
		asm volatile ( \
			"lv.q   C100, %1\n" \
			"lv.q   C110, %2\n" \
			"lv.q   C000, %3\n" \
			"vpfxs  " spfx "\n" \
			instr "." sizeq " " reg "000, " reg "100, " reg "110" \
			"\nsv.q   C000, %0\n" \
			: "+m" (*out) : "m" (plain), "m" (plain), "m" (marker) : "memory" \
		); \
	}
MARKED(m_vrcp_q, "q", "C", "vrcp", "-y, -x, -w, -z")
MARKED(m_vsin_q, "q", "C", "vsin", "-y, -x, -w, -z")
MARKED(m_vlog2_q, "q", "C", "vlog2", "y, x, w, z")
MARKED(m_vrcp_s, "s", "S", "vrcp", "y, y, y, y")
MARKED(m_vrcp_p, "p", "C", "vrcp", "z, w, x, y")
MARKED(m_vrcp_t, "t", "C", "vrcp", "x, y, w, z")
MARKED2(m_vadd_s, "s", "S", "vadd", "y, y, y, y")
MARKED2(m_vadd_p, "p", "C", "vadd", "z, w, x, y")
MARKED2(m_vadd_p2, "p", "C", "vadd", "x, w, x, y")
MARKED2(m_vadd_t, "t", "C", "vadd", "w, y, z, x")
MARKED2(m_vdiv_q, "q", "C", "vdiv", "-y, -x, -w, -z")
MARKED2(m_vmul_q, "q", "C", "vmul", "-y, -x, -w, -z")
// And position 0 holding something valid for a one-lane op: negate, abs, a constant.
MARKED(m_vrcp_negx, "q", "C", "vrcp", "-x, y, z, w")
MARKED(m_vrcp_absx, "q", "C", "vrcp", "|x|, y, z, w")
MARKED(m_vrcp_c2, "q", "C", "vrcp", "2, y, z, w")
MARKED(m_vrcp_cneghalf, "q", "C", "vrcp", "-1/2, y, z, w")
MARKED2(m_vdiv_negx, "q", "C", "vdiv", "-x, y, z, w")
MARKED2(m_vdiv_c2, "q", "C", "vdiv", "2, y, z, w")

static void show(const char *label, const ScePspFVector4 *v) {
	UVec4 u;
	memcpy(&u, v, 16);
	printf("  %-36s %08x,%08x,%08x,%08x\n", label, u.x, u.y, u.z, u.w);
}

#define RUN_SET(set, input) \
	for (int i = 0; i < ARRAY_SIZE(set); i++) { \
		set[i].func(&out, &input); \
		show(set[i].name, &out); \
	}

int main(int argc, char *argv[]) {
	ALIGN16 ScePspFVector4 out;

	printf("-- vadd.q x + 0 with d[0:1]: specials, zeros, edges --\n");
	vadd_sat01(&out, &specials); show("nan,-nan,inf,-inf", &out);
	vadd_sat01(&out, &zeros); show("0,-0,denorm,-denorm", &out);
	vadd_sat01(&out, &edges); show("1,-1,1+,-1-", &out);
	printf("-- vadd.q x + 0 with d[-1:1] --\n");
	vadd_sat11(&out, &specials); show("nan,-nan,inf,-inf", &out);
	vadd_sat11(&out, &zeros); show("0,-0,denorm,-denorm", &out);
	vadd_sat11(&out, &edges); show("1,-1,1+,-1-", &out);
	printf("-- vsat0.q / vsat1.q --\n");
	vsat0_q(&out, &specials); show("vsat0 nan,-nan,inf,-inf", &out);
	vsat0_q(&out, &zeros); show("vsat0 0,-0,denorm,-denorm", &out);
	vsat1_q(&out, &specials); show("vsat1 nan,-nan,inf,-inf", &out);
	vsat1_q(&out, &zeros); show("vsat1 0,-0,denorm,-denorm", &out);

	printf("-- vrcp.q of 1.5,-2.5,3.5,-4.5 --\n"); RUN_SET(vrcp_set, plain);
	printf("-- vrcp.q of nan,-nan,inf,-inf --\n"); RUN_SET(vrcp_set, specials);
	printf("-- vsin.q of 1.5,-2.5,3.5,-4.5 --\n"); RUN_SET(vsin_set, plain);
	printf("-- vsin.q of nan,-nan,inf,-inf --\n"); RUN_SET(vsin_set, specials);
	printf("-- vsqrt.q of 1.5,-2.5,3.5,-4.5 --\n"); RUN_SET(vsqrt_set, plain);
	printf("-- vlog2.q of 1.5,-2.5,3.5,-4.5 --\n"); RUN_SET(vlog2_set, plain);
	printf("-- vexp2.q of 1.5,-2.5,3.5,-4.5 --\n"); RUN_SET(vexp2_set, plain);
	printf("-- vdiv.q of 1.5,-2.5,3.5,-4.5 by itself --\n"); RUN_SET(vdiv_set, plain);
	printf("-- vdiv.q of nan,-nan,inf,-inf by 1.5,-2.5,3.5,-4.5 --\n"); RUN_SET(vdiv_set, specials);

	printf("-- out-of-range swizzles with the destination preset to 42,43,44,45 --\n");
	m_vrcp_q(&out); show("vrcp.q s[-y,-x,-w,-z]", &out);
	m_vsin_q(&out); show("vsin.q s[-y,-x,-w,-z]", &out);
	m_vlog2_q(&out); show("vlog2.q s[y,x,w,z]", &out);
	m_vrcp_s(&out); show("vrcp.s s[y,y,y,y]", &out);
	m_vrcp_p(&out); show("vrcp.p s[z,w,x,y]", &out);
	m_vrcp_t(&out); show("vrcp.t s[x,y,w,z]", &out);
	m_vadd_s(&out); show("vadd.s s[y,y,y,y]", &out);
	m_vadd_p(&out); show("vadd.p s[z,w,x,y]", &out);
	m_vadd_p2(&out); show("vadd.p s[x,w,x,y]", &out);
	m_vadd_t(&out); show("vadd.t s[w,y,z,x]", &out);
	m_vdiv_q(&out); show("vdiv.q s[-y,-x,-w,-z]", &out);
	m_vmul_q(&out); show("vmul.q s[-y,-x,-w,-z]", &out);
	m_vrcp_negx(&out); show("vrcp.q s[-x,y,z,w]", &out);
	m_vrcp_absx(&out); show("vrcp.q s[|x|,y,z,w]", &out);
	m_vrcp_c2(&out); show("vrcp.q s[2,y,z,w]", &out);
	m_vrcp_cneghalf(&out); show("vrcp.q s[-1/2,y,z,w]", &out);
	m_vdiv_negx(&out); show("vdiv.q s[-x,y,z,w]", &out);
	m_vdiv_c2(&out); show("vdiv.q s[2,y,z,w]", &out);
	return 0;
}
