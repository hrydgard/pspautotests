#include <common.h>
#include "vfpu_common.h"
#include <string.h>

// The prefix control registers ($128-$130) read and written directly with mfvc/mtvc, instead of
// through vpfxs/vpfxt/vpfxd. What does each vpfx encoding read back as, does a prefix written
// with mtvc apply the same way, which bits of a written value stick, and what happens with a
// prefix that refers to lanes the op doesn't have.

typedef struct { unsigned int x, y, z, w; } UVec4;

static ALIGN16 const ScePspFVector4 inS = { 1.5f, -2.5f, 3.5f, -4.5f };
static ALIGN16 const ScePspFVector4 inT = { 10.0f, 20.0f, 30.0f, 40.0f };

// Set with vpfxs, read back with mfvc.
#define READBACK(name, pfx) \
	static unsigned int __attribute__((noinline)) name(void) { \
		unsigned int v; \
		asm volatile ( \
			pfx "\n" \
			"mfvc  %0, $128\n" \
			"vnop\n" \
			"vmov.q C000, C000\n" \
			: "=r" (v) : : "memory" \
		); \
		return v; \
	}

READBACK(rb_default,  "vmov.q C000, C000")
READBACK(rb_xyzw,     "vpfxs x, y, z, w")
READBACK(rb_wzyx,     "vpfxs w, z, y, x")
READBACK(rb_xxxx,     "vpfxs x, x, x, x")
READBACK(rb_neg,      "vpfxs -x, -y, -z, -w")
READBACK(rb_abs,      "vpfxs |x|, |y|, |z|, |w|")
READBACK(rb_negabs,   "vpfxs -|x|, -|y|, -|z|, -|w|")
READBACK(rb_c0123,    "vpfxs 0, 1, 2, 3")
READBACK(rb_cfrac,    "vpfxs 1/2, 1/3, 1/4, 1/6")
READBACK(rb_cneg,     "vpfxs -1, -2, -3, -1/2")
READBACK(rb_mixed,    "vpfxs -y, |w|, 2, -1/3")

static unsigned int __attribute__((noinline)) rb_t(void) {
	unsigned int v;
	asm volatile ("vpfxt 1, 1/2, 3, 0\nmfvc %0, $129\nvmov.q C000, C000\n" : "=r" (v) : : "memory");
	return v;
}

#define READBACK_D(name, pfx) \
	static unsigned int __attribute__((noinline)) name(void) { \
		unsigned int v; \
		asm volatile (pfx "\nmfvc %0, $130\nvmov.q C000, C000\n" : "=r" (v) : : "memory"); \
		return v; \
	}
READBACK_D(rbd_sat01,  "vpfxd 0:1, 0:1, 0:1, 0:1")
READBACK_D(rbd_sat11,  "vpfxd -1:1, -1:1, -1:1, -1:1")
READBACK_D(rbd_mask,   "vpfxd m, m, m, m")
READBACK_D(rbd_mixed,  "vpfxd 0:1, m, -1:1, m")
READBACK_D(rbd_mixed2, "vpfxd m, -1:1, 0:1, m")

// Write a raw value with mtvc, read it back, and run an op with it pending.
static void __attribute__((noinline)) via_mtvc(unsigned int spfx, unsigned int tpfx, unsigned int dpfx, unsigned int *readback, ScePspFVector4 *out) {
	asm volatile (
		"lv.q   C100, %6\n"
		"lv.q   C110, %7\n"
		"mtvc   %3, $128\n"
		"mtvc   %4, $129\n"
		"mtvc   %5, $130\n"
		"mfvc   $t0, $128\n"
		"mfvc   $t1, $129\n"
		"mfvc   $t2, $130\n"
		"sw     $t0, 0(%1)\n"
		"sw     $t1, 4(%1)\n"
		"sw     $t2, 8(%1)\n"
		"vadd.q C000, C100, C110\n"
		"sv.q   C000, %0\n"
		: "+m" (*out) : "r" (readback), "m" (*readback), "r" (spfx), "r" (tpfx), "r" (dpfx), "m" (inS), "m" (inT) : "t0", "t1", "t2", "memory"
	);
}

// A prefix naming lanes beyond the op's size.
#define OUTSIDE(name, size, reg, spfx) \
	static void __attribute__((noinline)) name(ScePspFVector4 *out) { \
		asm volatile ( \
			"lv.q   C100, %1\n" \
			"lv.q   C110, %2\n" \
			"vmov.q C000, C000[0, 0, 0, 0]\n" \
			"vpfxs  " spfx "\n" \
			"vadd." size " " reg "000, " reg "100, " reg "110\n" \
			"sv.q   C000, %0\n" \
			: "+m" (*out) : "m" (inS), "m" (inT) : "memory" \
		); \
	}

OUTSIDE(out_s_y,    "s", "S", "y, y, y, y")
OUTSIDE(out_s_w,    "s", "S", "w, w, w, w")
OUTSIDE(out_s_negw, "s", "S", "-|w|, x, x, x")
OUTSIDE(out_p_zw,   "p", "C", "z, w, x, y")
OUTSIDE(out_p_wz,   "p", "C", "w, z, y, x")
OUTSIDE(out_t_w,    "t", "C", "w, w, w, x")
OUTSIDE(out_t_neg,  "t", "C", "x, y, z, -w")
OUTSIDE(out_p_hi,   "p", "C", "x, y, -z, |w|")

static void show(const char *label, const ScePspFVector4 *v) {
	UVec4 u;
	memcpy(&u, v, 16);
	printf("%-30s %08x,%08x,%08x,%08x\n", label, u.x, u.y, u.z, u.w);
}

int main(int argc, char *argv[]) {
	ALIGN16 ScePspFVector4 out;
	ALIGN16 unsigned int rb[4];

	printf("-- vpfxs encodings, read back --\n");
	printf("default:      %08x\n", rb_default());
	printf("x,y,z,w:      %08x\n", rb_xyzw());
	printf("w,z,y,x:      %08x\n", rb_wzyx());
	printf("x,x,x,x:      %08x\n", rb_xxxx());
	printf("-x,-y,-z,-w:  %08x\n", rb_neg());
	printf("|x|..|w|:     %08x\n", rb_abs());
	printf("-|x|..-|w|:   %08x\n", rb_negabs());
	printf("0,1,2,3:      %08x\n", rb_c0123());
	printf("1/2,1/3,1/4,1/6: %08x\n", rb_cfrac());
	printf("-1,-2,-3,-1/2: %08x\n", rb_cneg());
	printf("-y,|w|,2,-1/3: %08x\n", rb_mixed());
	printf("vpfxt 1,1/2,3,0: %08x\n", rb_t());
	printf("-- vpfxd encodings, read back --\n");
	printf("0:1 x4:       %08x\n", rbd_sat01());
	printf("-1:1 x4:      %08x\n", rbd_sat11());
	printf("m x4:         %08x\n", rbd_mask());
	printf("0:1,m,-1:1,m:  %08x\n", rbd_mixed());
	printf("m,-1:1,0:1,m: %08x\n", rbd_mixed2());

	printf("-- written with mtvc --\n");
	// The same three prefixes prefix_consume.c uses, as raw values.
	via_mtvc(0x000f00b1, 0x0000f40d, 0x00000055, rb, &out);
	printf("readback: %08x,%08x,%08x\n", rb[0], rb[1], rb[2]); show("vadd.q with them", &out);
	// Every bit set: which ones stick?
	via_mtvc(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, rb, &out);
	printf("readback: %08x,%08x,%08x\n", rb[0], rb[1], rb[2]); show("vadd.q with all bits", &out);
	// Only the high (undocumented) bits set.
	via_mtvc(0xFFF00000, 0xFFF00000, 0xFFFFFF00, rb, &out);
	printf("readback: %08x,%08x,%08x\n", rb[0], rb[1], rb[2]); show("vadd.q with high bits", &out);
	// Defaults, explicitly.
	via_mtvc(0x000000e4, 0x000000e4, 0x00000000, rb, &out);
	printf("readback: %08x,%08x,%08x\n", rb[0], rb[1], rb[2]); show("vadd.q with defaults", &out);
	// Zero, which is not the default for S/T (swizzle x,x,x,x).
	via_mtvc(0, 0, 0, rb, &out);
	printf("readback: %08x,%08x,%08x\n", rb[0], rb[1], rb[2]); show("vadd.q with zeros", &out);

	printf("-- prefix lanes outside the op size --\n");
	out_s_y(&out);    show("vadd.s [y,y,y,y]", &out);
	out_s_w(&out);    show("vadd.s [w,w,w,w]", &out);
	out_s_negw(&out); show("vadd.s [-|w|,x,x,x]", &out);
	out_p_zw(&out);   show("vadd.p [z,w,x,y]", &out);
	out_p_wz(&out);   show("vadd.p [w,z,y,x]", &out);
	out_t_w(&out);    show("vadd.t [w,w,w,x]", &out);
	out_t_neg(&out);  show("vadd.t [x,y,z,-w]", &out);
	out_p_hi(&out);   show("vadd.p [x,y,-z,|w|]", &out);
	return 0;
}
