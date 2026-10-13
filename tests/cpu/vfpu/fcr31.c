#include <common.h>
#include "vfpu_common.h"

// Whether the FPU's control register (fcr31: rounding mode in bits 0-1, flush-to-zero in bit 24)
// changes VFPU results. The inputs make every op inexact or denormal somewhere, so a VFPU that
// follows fcr31 shows up as lines that differ between the sections. vmin/vmax with denormals
// against zeros are here too, since flush-to-zero would change which bits they return.

#define FS 0x01000000

static void setFcr31(unsigned int v) {
	asm volatile ("ctc1 %0, $31\nnop\n" : : "r" (v));
}

typedef struct { unsigned int v[4]; } __attribute__((aligned(16))) UVec4;

#define BINOP(name, instr) \
	static void NOINLINE name(UVec4 *out, const UVec4 *s, const UVec4 *t) { \
		asm volatile ( \
			"lv.q C100, %1\n" \
			"lv.q C200, %2\n" \
			instr "\n" \
			"sv.q C000, %0\n" \
			: "+m" (*out) : "m" (*s), "m" (*t) : "memory" \
		); \
	}

BINOP(vadd_q, "vadd.q C000, C100, C200")
BINOP(vsub_q, "vsub.q C000, C100, C200")
BINOP(vmul_q, "vmul.q C000, C100, C200")
BINOP(vdiv_q, "vdiv.q C000, C100, C200")
BINOP(vmin_q, "vmin.q C000, C100, C200")
BINOP(vmax_q, "vmax.q C000, C100, C200")
BINOP(vscl_q, "vscl.q C000, C100, S200")
BINOP(vdot_q, "vzero.q C000\nvdot.q S000, C100, C200")
BINOP(vhdp_q, "vzero.q C000\nvhdp.q S000, C100, C200")
BINOP(vcrs_t, "vzero.q C000\nvcrs.t C000, C100, C200")

#define UNOP(name, instr) \
	static void NOINLINE name(UVec4 *out, const UVec4 *s, const UVec4 *t) { \
		asm volatile ( \
			"lv.q C100, %1\n" \
			instr "\n" \
			"sv.q C000, %0\n" \
			: "+m" (*out) : "m" (*s) : "memory" \
		); \
	}

UNOP(vmov_q, "vmov.q C000, C100")
UNOP(vabs_q, "vabs.q C000, C100")
UNOP(vneg_q, "vneg.q C000, C100")
UNOP(vsqrt_q, "vsqrt.q C000, C100")
UNOP(vrcp_q, "vrcp.q C000, C100")
UNOP(vrsq_q, "vrsq.q C000, C100")
UNOP(vfad_q, "vzero.q C000\nvfad.q S000, C100")
UNOP(vavg_q, "vzero.q C000\nvavg.q S000, C100")
UNOP(vf2in_q, "vf2in.q C000, C100, 0")
UNOP(vf2iz_q, "vf2iz.q C000, C100, 0")
UNOP(vf2iu_q, "vf2iu.q C000, C100, 0")
UNOP(vf2id_q, "vf2id.q C000, C100, 0")
UNOP(vi2f_q, "vi2f.q C000, C100, 0")
UNOP(vf2h_q, "vzero.q C000\nvf2h.q C000, C100")

typedef void (*OpFunc)(UVec4 *, const UVec4 *, const UVec4 *);

// Inexact: 1 + 1.5 ulp / 2, -1 - 1.5 ulp / 2, 1 / 3, -1 / 3.
static const UVec4 inexactS = {{ 0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000 }};
static const UVec4 inexactT = {{ 0x33c00000, 0xb3c00000, 0x40400000, 0x40400000 }};
// Denormal results from normal inputs, and denormal inputs.
static const UVec4 denormS = {{ 0x00800000, 0x80ffffff, 0x00400000, 0x80000001 }};
static const UVec4 denormT = {{ 0x3f000000, 0x3f000000, 0x3f800000, 0x3f800000 }};
// Denormals against zeros, for vmin/vmax.
static const UVec4 zeroS = {{ 0x00400000, 0x80400000, 0x00000000, 0x80000000 }};
static const UVec4 zeroT = {{ 0x00000000, 0x80000000, 0x80400000, 0x00400000 }};
// Half-way conversions.
static const UVec4 halfS = {{ 0x3f000000, 0x3fc00000, 0xbf000000, 0x40200000 }};
// Ints that don't fit in 24 bits.
static const UVec4 bigInts = {{ 16777217, (unsigned int)-16777217, 0x7fffffff, 16777219 }};
// Unary inputs: 3, 0.1 (sqrt and rcp are inexact), a denormal, -denormal.
static const UVec4 unaryS = {{ 0x40400000, 0x3dcccccd, 0x00400000, 0x80000001 }};
// Half floats: values that round differently in each mode.
static const UVec4 halfFloatS = {{ 0x3f801000, 0xbf801000, 0x3f802000, 0x33000001 }};

static const struct {
	const char *name;
	OpFunc func;
	const UVec4 *s;
	const UVec4 *t;
} cases[] = {
	{ "vadd inexact", vadd_q, &inexactS, &inexactT },
	{ "vsub inexact", vsub_q, &inexactS, &inexactT },
	{ "vmul inexact", vmul_q, &inexactT, &inexactT },
	{ "vdiv inexact", vdiv_q, &inexactS, &inexactT },
	{ "vscl inexact", vscl_q, &inexactT, &inexactT },
	{ "vdot inexact", vdot_q, &inexactS, &inexactT },
	{ "vhdp inexact", vhdp_q, &inexactS, &inexactT },
	{ "vcrs inexact", vcrs_t, &inexactS, &inexactT },
	{ "vadd denorm ", vadd_q, &denormS, &denormT },
	{ "vsub denorm ", vsub_q, &denormS, &denormT },
	{ "vmul denorm ", vmul_q, &denormS, &denormT },
	{ "vdiv denorm ", vdiv_q, &denormS, &denormT },
	{ "vdot denorm ", vdot_q, &denormS, &denormT },
	{ "vmin zero   ", vmin_q, &zeroS, &zeroT },
	{ "vmax zero   ", vmax_q, &zeroS, &zeroT },
	{ "vmin denorm ", vmin_q, &denormS, &zeroS },
	{ "vmax denorm ", vmax_q, &denormS, &zeroS },
	{ "vmov        ", vmov_q, &denormS, NULL },
	{ "vabs        ", vabs_q, &denormS, NULL },
	{ "vneg        ", vneg_q, &denormS, NULL },
	{ "vsqrt       ", vsqrt_q, &unaryS, NULL },
	{ "vrcp        ", vrcp_q, &unaryS, NULL },
	{ "vrsq        ", vrsq_q, &unaryS, NULL },
	{ "vfad        ", vfad_q, &inexactT, NULL },
	{ "vavg        ", vavg_q, &inexactT, NULL },
	{ "vf2in       ", vf2in_q, &halfS, NULL },
	{ "vf2iz       ", vf2iz_q, &halfS, NULL },
	{ "vf2iu       ", vf2iu_q, &halfS, NULL },
	{ "vf2id       ", vf2id_q, &halfS, NULL },
	{ "vf2in denorm", vf2in_q, &denormS, NULL },
	{ "vf2iu denorm", vf2iu_q, &denormS, NULL },
	{ "vf2id denorm", vf2id_q, &denormS, NULL },
	{ "vi2f        ", vi2f_q, &bigInts, NULL },
	{ "vf2h        ", vf2h_q, &halfFloatS, NULL },
};

static const struct {
	const char *name;
	unsigned int fcr31;
} modes[] = {
	{ "nearest", 0 },
	{ "zero", 1 },
	{ "up", 2 },
	{ "down", 3 },
	{ "nearest, FS", FS | 0 },
	{ "down, FS", FS | 3 },
};

int main(int argc, char *argv[]) {
	for (int m = 0; m < ARRAY_SIZE(modes); m++) {
		printf("== fcr31 %08x (%s) ==\n", modes[m].fcr31, modes[m].name);
		for (int i = 0; i < ARRAY_SIZE(cases); i++) {
			UVec4 out = {{ 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef }};
			setFcr31(modes[m].fcr31);
			cases[i].func(&out, cases[i].s, cases[i].t);
			setFcr31(0);
			printf("%s: %08x %08x %08x %08x\n", cases[i].name, out.v[0], out.v[1], out.v[2], out.v[3]);
		}
	}
	return 0;
}
