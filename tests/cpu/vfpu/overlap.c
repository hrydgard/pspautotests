#include <common.h>
#include "vfpu_common.h"
#include <string.h>

// Destination registers overlapping the sources. On hardware every op reads all its inputs before
// writing anything, which matters when the destination is one of the sources, or shares some
// registers with one (a column against a row, or a triple against the triple one row down). JITs
// decide per op whether they need temporaries for this, so each op class is here with the overlaps
// that can happen to it. Every case starts from the same matrix M000 (1..16 as floats, in column
// order) plus M100 (101..116) for a second operand, and prints all of M000 afterwards.

typedef struct { unsigned int v[16]; } Mat;

static ALIGN16 const float base[16] = {
	1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f,
};
static ALIGN16 const float other[16] = {
	101.0f, 102.0f, 103.0f, 104.0f, 105.0f, 106.0f, 107.0f, 108.0f, 109.0f, 110.0f, 111.0f, 112.0f, 113.0f, 114.0f, 115.0f, 116.0f,
};

#define CASE(name, ops) \
	static void __attribute__((noinline)) name(Mat *out) { \
		asm volatile ( \
			".set noreorder\n" \
			"lv.q   C000, 0(%1)\n" \
			"lv.q   C010, 16(%1)\n" \
			"lv.q   C020, 32(%1)\n" \
			"lv.q   C030, 48(%1)\n" \
			"lv.q   C100, 0(%2)\n" \
			"lv.q   C110, 16(%2)\n" \
			"lv.q   C120, 32(%2)\n" \
			"lv.q   C130, 48(%2)\n" \
			ops \
			"sv.q   C000, 0(%0)\n" \
			"sv.q   C010, 16(%0)\n" \
			"sv.q   C020, 32(%0)\n" \
			"sv.q   C030, 48(%0)\n" \
			".set reorder\n" \
			: : "r" (out), "r" (base), "r" (other) : "memory" \
		); \
	}

// Three-operand vector ops: d = s, d = t, d = s = t, d shifted one row into s.
CASE(vadd_dds,   "vadd.q C000, C000, C100\n")
CASE(vadd_ddt,   "vadd.q C000, C100, C000\n")
CASE(vadd_ddd,   "vadd.q C000, C000, C000\n")
CASE(vadd_row,   "vadd.q R000, C000, C100\n")     // d = row 0, s = column 0: share S000
CASE(vadd_row2,  "vadd.q C000, R000, C100\n")
CASE(vadd_shift, "vadd.t C010, C000, C100\n")     // d = rows 1-3, s = rows 0-2 of column 0
CASE(vadd_shift2, "vadd.t C000, C010, C100\n")
CASE(vmul_row,   "vmul.p R000, C000, C100\n")
CASE(vsub_row,   "vsub.p C000, R000, C100\n")
CASE(vmin_dds,   "vmin.q C000, C000, C100\n")
CASE(vscl_dds,   "vscl.q C000, C000, S100\n")
CASE(vscl_dt,    "vscl.q C000, C100, S000\n")     // the scalar is inside d
CASE(vscl_dt2,   "vscl.q C000, C100, S030\n")     // the scalar is the last lane of d

// Dot products: the single destination inside a source.
CASE(vdot_ds0,   "vdot.q S000, C000, C100\n")
CASE(vdot_ds3,   "vdot.q S030, C000, C100\n")
CASE(vdot_dt,    "vdot.q S000, C100, C000\n")
CASE(vhdp_ds,    "vhdp.q S000, C000, C100\n")
CASE(vhdp_ds3,   "vhdp.q S030, C000, C100\n")
CASE(vdet_ds,    "vdet.p S000, C000, C100\n")

// Cross product. (vcrsp, vqmul, vrot and every matrix op refuse an overlapping destination in the
// assembler - "destination register conflict" - so those can't occur in a compiled game and aren't
// here. vmscl with the scalar inside the destination is refused too.)
CASE(vcrs_dds,   "vcrs.t C000, C000, C100\n")
CASE(vcrs_ddt,   "vcrs.t C000, C100, C000\n")
CASE(vcrs_ddd,   "vcrs.t C000, C000, C000\n")
CASE(vcrs_shift, "vcrs.t C010, C000, C100\n")
CASE(vcrsp_shift, "vcrsp.t C010, C000, C100\n")

// Unary ops with a shifted destination: the moving-window copy.
CASE(vmov_up,    "vmov.t C010, C000\n")
CASE(vmov_down,  "vmov.t C000, C010\n")
CASE(vmov_row,   "vmov.q R000, C000\n")
CASE(vneg_up,    "vneg.t C010, C000\n")
CASE(vabs_row,   "vabs.p R000, C000\n")
CASE(vsat1_up,   "vsat1.t C010, C000\n")
CASE(vsat0_row,  "vsat0.q R000, C000\n")
CASE(vsgn_up,    "vsgn.t C010, C000\n")
CASE(vocp_up,    "vocp.t C010, C000\n")

// Butterflies, sorts and rotation in place and shifted.
CASE(vbfy1_dd,   "vbfy1.q C000, C000\n")
CASE(vbfy1_row,  "vbfy1.q R000, C000\n")
CASE(vbfy2_dd,   "vbfy2.q C000, C000\n")
CASE(vsrt1_dd,   "vsrt1.q C000, C000\n")
CASE(vsrt1_row,  "vsrt1.q R000, C000\n")
CASE(vsrt2_dd,   "vsrt2.q C000, C000\n")
CASE(vsrt3_dd,   "vsrt3.q C000, C000\n")
CASE(vsrt4_dd,   "vsrt4.q C000, C000\n")

// Conversions that change the size, so the destination and source only partly line up.
CASE(vf2h_dd,    "vf2h.q C000, C000\n")          // quad in, pair out at C000
CASE(vf2h_shift, "vf2h.q C010, C000\n")
CASE(vh2f_dd,    "vh2f.p C000, C000\n")          // pair in, quad out
CASE(vh2f_shift, "vh2f.p C000, C010\n")
CASE(vi2c_dd,    "vi2c.q S000, C000\n")
CASE(vi2c_d3,    "vi2c.q S030, C000\n")
CASE(vi2s_dd,    "vi2s.q C000, C000\n")
CASE(vi2s_shift, "vi2s.q C020, C000\n")
CASE(vs2i_dd,    "vs2i.p C000, C000\n")
CASE(vs2i_shift, "vs2i.p C000, C020\n")
CASE(vuc2i_dd,   "vuc2ifs.s C000, S000\n")
CASE(vuc2i_d3,   "vuc2ifs.s C000, S030\n")
CASE(vsocp_d,    "vsocp.p C000, C000\n")
CASE(vf2iz_dd,   "vf2iz.q C000, C000, 4\n")
CASE(vf2iz_row,  "vf2iz.q R000, C000, 4\n")
CASE(vi2f_dd,    "vi2f.q C000, C000, 4\n")
CASE(vt4444_dd,  "vt4444.q C000, C000\n")
CASE(vt5551_dd,  "vt5551.q C000, C000\n")
CASE(vt5650_dd,  "vt5650.q C000, C000\n")
CASE(vt4444_shift, "vt4444.q C010, C000\n")

// Matrix ops: only the ones the assembler allows.
CASE(vmscl_dd,   "vmscl.q M000, M000, S100\n")

typedef void (*CaseFunc)(Mat *);
#define ENTRY(f, text) { #f, text, f }
static const struct { const char *name; const char *text; CaseFunc func; } cases[] = {
	ENTRY(vadd_dds, "vadd.q C000, C000, C100"), ENTRY(vadd_ddt, "vadd.q C000, C100, C000"), ENTRY(vadd_ddd, "vadd.q C000, C000, C000"),
	ENTRY(vadd_row, "vadd.q R000, C000, C100"), ENTRY(vadd_row2, "vadd.q C000, R000, C100"),
	ENTRY(vadd_shift, "vadd.t C010, C000, C100"), ENTRY(vadd_shift2, "vadd.t C000, C010, C100"),
	ENTRY(vmul_row, "vmul.p R000, C000, C100"), ENTRY(vsub_row, "vsub.p C000, R000, C100"),
	ENTRY(vmin_dds, "vmin.q C000, C000, C100"),
	ENTRY(vscl_dds, "vscl.q C000, C000, S100"), ENTRY(vscl_dt, "vscl.q C000, C100, S000"), ENTRY(vscl_dt2, "vscl.q C000, C100, S030"),
	ENTRY(vdot_ds0, "vdot.q S000, C000, C100"), ENTRY(vdot_ds3, "vdot.q S030, C000, C100"), ENTRY(vdot_dt, "vdot.q S000, C100, C000"),
	ENTRY(vhdp_ds, "vhdp.q S000, C000, C100"), ENTRY(vhdp_ds3, "vhdp.q S030, C000, C100"), ENTRY(vdet_ds, "vdet.p S000, C000, C100"),
	ENTRY(vcrs_dds, "vcrs.t C000, C000, C100"), ENTRY(vcrs_ddt, "vcrs.t C000, C100, C000"), ENTRY(vcrs_ddd, "vcrs.t C000, C000, C000"), ENTRY(vcrs_shift, "vcrs.t C010, C000, C100"),
	ENTRY(vcrsp_shift, "vcrsp.t C010, C000, C100"),
	
	ENTRY(vmov_up, "vmov.t C010, C000"), ENTRY(vmov_down, "vmov.t C000, C010"), ENTRY(vmov_row, "vmov.q R000, C000"),
	ENTRY(vneg_up, "vneg.t C010, C000"), ENTRY(vabs_row, "vabs.p R000, C000"), ENTRY(vsat1_up, "vsat1.t C010, C000"),
	ENTRY(vsat0_row, "vsat0.q R000, C000"), ENTRY(vsgn_up, "vsgn.t C010, C000"), ENTRY(vocp_up, "vocp.t C010, C000"),
	ENTRY(vbfy1_dd, "vbfy1.q C000, C000"), ENTRY(vbfy1_row, "vbfy1.q R000, C000"), ENTRY(vbfy2_dd, "vbfy2.q C000, C000"),
	ENTRY(vsrt1_dd, "vsrt1.q C000, C000"), ENTRY(vsrt1_row, "vsrt1.q R000, C000"), ENTRY(vsrt2_dd, "vsrt2.q C000, C000"),
	ENTRY(vsrt3_dd, "vsrt3.q C000, C000"), ENTRY(vsrt4_dd, "vsrt4.q C000, C000"),
	
	ENTRY(vf2h_dd, "vf2h.q C000, C000"), ENTRY(vf2h_shift, "vf2h.q C010, C000"), ENTRY(vh2f_dd, "vh2f.p C000, C000"), ENTRY(vh2f_shift, "vh2f.p C000, C010"),
	ENTRY(vi2c_dd, "vi2c.q S000, C000"), ENTRY(vi2c_d3, "vi2c.q S030, C000"), ENTRY(vi2s_dd, "vi2s.q C000, C000"), ENTRY(vi2s_shift, "vi2s.q C020, C000"),
	ENTRY(vs2i_dd, "vs2i.p C000, C000"), ENTRY(vs2i_shift, "vs2i.p C000, C020"), ENTRY(vuc2i_dd, "vuc2ifs.s C000, S000"), ENTRY(vuc2i_d3, "vuc2ifs.s C000, S030"),
	ENTRY(vsocp_d, "vsocp.p C000, C000"),
	ENTRY(vf2iz_dd, "vf2iz.q C000, C000, 4"), ENTRY(vf2iz_row, "vf2iz.q R000, C000, 4"), ENTRY(vi2f_dd, "vi2f.q C000, C000, 4"),
	ENTRY(vt4444_dd, "vt4444.q C000, C000"), ENTRY(vt5551_dd, "vt5551.q C000, C000"), ENTRY(vt5650_dd, "vt5650.q C000, C000"), ENTRY(vt4444_shift, "vt4444.q C010, C000"),
	
	
	
	ENTRY(vmscl_dd, "vmscl.q M000, M000, S100"), 
	
	
	
	
	
};

int main(int argc, char *argv[]) {
	ALIGN16 Mat m;
	printf("M000 before, by column: 1..16   M100: 101..116\n");
	for (int i = 0; i < ARRAY_SIZE(cases); i++) {
		memset(&m, 0xCC, sizeof(m));
		cases[i].func(&m);
		printf("%-34s", cases[i].text);
		for (int c = 0; c < 4; c++) {
			printf(" %08x,%08x,%08x,%08x", m.v[c * 4], m.v[c * 4 + 1], m.v[c * 4 + 2], m.v[c * 4 + 3]);
		}
		printf("\n");
	}
	return 0;
}
