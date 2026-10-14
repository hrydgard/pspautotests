#include <common.h>
#include "vfpu_common.h"

// What S and T prefixes do in the lanes past an instruction's vector size, for the instructions
// that reduce their lanes to one value (the dot products and their relatives). A constant or a
// swizzle in an unused lane might be ignored, or might take part. The inputs make each possible
// answer different:
//   S = C100 = { 1, 2, 3, 9 }, T = C200 = { 1, 1, 1, 5 }
// so a .t dot of the first three lanes is 6, a constant pair in lane 3 such as 3 * 1/2 adds 1.5, and
// the registers' own fourth lanes add 9 * 5 = 45.

static ALIGN16 const ScePspFVector4 inS = { 1.0f, 2.0f, 3.0f, 9.0f };
static ALIGN16 const ScePspFVector4 inT = { 1.0f, 1.0f, 1.0f, 5.0f };

#define LANES_TEST(name, ops) \
	static void NOINLINE name(ScePspFVector4 *r) { \
		asm volatile ( \
			"lv.q   C100, %1\n" \
			"lv.q   C200, %2\n" \
			"vmov.q C000, C000[0, 0, 0, 0]\n" \
			ops \
			"sv.q   C000, %0\n" \
			: "=m" (*r) : "m" (inS), "m" (inT) : "memory" \
		); \
	}

// No prefixes, for reference.
LANES_TEST(t_vdot_t, "vdot.t S000, C100, C200\n")
LANES_TEST(t_vdot_q, "vdot.q S000, C100, C200\n")
// Constants in the unused lane of S and T.
LANES_TEST(t_vdot_t_const, "vpfxs x, y, z, 3\n" "vpfxt x, y, z, 1/2\n" "vdot.t S000, C100, C200\n")
LANES_TEST(t_vdot_t_one, "vpfxs x, y, z, 1\n" "vpfxt x, y, z, 1\n" "vdot.t S000, C100, C200\n")
// A constant in S only, T only, and both lanes reading the registers' fourth lanes.
LANES_TEST(t_vdot_t_sconst, "vpfxs x, y, z, 3\n" "vdot.t S000, C100, C200\n")
LANES_TEST(t_vdot_t_tconst, "vpfxt x, y, z, 1/2\n" "vdot.t S000, C100, C200\n")
LANES_TEST(t_vdot_t_w, "vpfxs x, y, z, w\n" "vpfxt x, y, z, w\n" "vdot.t S000, C100, C200\n")
// The default swizzle reads lane 3 as w too, so a prefix that only changes an earlier lane.
LANES_TEST(t_vdot_t_neg, "vpfxs -x, y, z, w\n" "vdot.t S000, C100, C200\n")
// Two unused lanes in .p, one in .s.
LANES_TEST(t_vdot_p_const, "vpfxs x, y, 3, 1/2\n" "vpfxt x, y, 2, 1\n" "vdot.p S000, C100, C200\n")
LANES_TEST(t_vdot_s_const, "vpfxs x, 3, 1/2, 2\n" "vpfxt x, 1/2, 1, 1\n" ".word 0x64880400\n")  // vdot.s S000, S100, S200, which the assembler refuses
// vhdp, whose last S lane is a 1, and the sums.
LANES_TEST(t_vhdp_t, "vhdp.t S000, C100, C200\n")
LANES_TEST(t_vhdp_t_const, "vpfxs x, y, z, 3\n" "vpfxt x, y, z, 1/2\n" "vhdp.t S000, C100, C200\n")
LANES_TEST(t_vfad_t, "vfad.t S000, C100\n")
LANES_TEST(t_vfad_t_const, "vpfxs x, y, z, 3\n" "vfad.t S000, C100\n")
LANES_TEST(t_vavg_t_const, "vpfxs x, y, z, 3\n" "vavg.t S000, C100\n")
// vdet.p, a two lane cross product: s.x * t.y - s.y * t.x.
LANES_TEST(t_vdet_p, "vdet.p S000, C100, C200\n")
LANES_TEST(t_vdet_p_const, "vpfxs x, y, 3, 1/2\n" "vpfxt x, y, 2, 1\n" "vdet.p S000, C100, C200\n")

struct Test {
	const char *name;
	void (*func)(ScePspFVector4 *r);
};

static const struct Test tests[] = {
	{ "vdot.t", &t_vdot_t },
	{ "vdot.q", &t_vdot_q },
	{ "vdot.t s=[x,y,z,3] t=[x,y,z,1/2]", &t_vdot_t_const },
	{ "vdot.t s=[x,y,z,1] t=[x,y,z,1]", &t_vdot_t_one },
	{ "vdot.t s=[x,y,z,3]", &t_vdot_t_sconst },
	{ "vdot.t t=[x,y,z,1/2]", &t_vdot_t_tconst },
	{ "vdot.t s=[x,y,z,w] t=[x,y,z,w]", &t_vdot_t_w },
	{ "vdot.t s=[-x,y,z,w]", &t_vdot_t_neg },
	{ "vdot.p s=[x,y,3,1/2] t=[x,y,2,1]", &t_vdot_p_const },
	{ "vdot.s s=[x,3,1/2,2] t=[x,1/2,1,1]", &t_vdot_s_const },
	{ "vhdp.t", &t_vhdp_t },
	{ "vhdp.t s=[x,y,z,3] t=[x,y,z,1/2]", &t_vhdp_t_const },
	{ "vfad.t", &t_vfad_t },
	{ "vfad.t s=[x,y,z,3]", &t_vfad_t_const },
	{ "vavg.t s=[x,y,z,3]", &t_vavg_t_const },
	{ "vdet.p", &t_vdet_p },
	{ "vdet.p s=[x,y,3,1/2] t=[x,y,2,1]", &t_vdet_p_const },
};

int main(int argc, char *argv[]) {
	for (size_t i = 0; i < ARRAY_SIZE(tests); i++) {
		ALIGN16 ScePspFVector4 r;
		tests[i].func(&r);
		unsigned int bits;
		memcpy(&bits, &r.x, sizeof(bits));
		printf("%-36s %08x  %f\n", tests[i].name, bits, r.x);
	}
	return 0;
}
