#include <common.h>
#include "vfpu_common.h"
#include <string.h>

// What a pending D prefix does to the loads into VFPU registers (and mtv), which prefix_consume
// shows don't consume it. Each test fills M000 with a pattern, sets a D prefix, runs one load and
// prints M000 and the prefix control registers; a vmov.q on C700 then consumes the prefix. The
// prefix saturates some lanes and masks others, so both show if applied:
//   D: [0:1, m, -1:1, m]  - lanes 1 and 3 masked
//   D1: [m, 0:1, m, 0:1]  - lane 0 masked, for the single loads

static ALIGN16 const ScePspFVector4 pattern[4] = {
	{ 7.0f, 8.0f, 9.0f, 10.0f },
	{ 11.0f, 12.0f, 13.0f, 14.0f },
	{ 15.0f, 16.0f, 17.0f, 18.0f },
	{ 19.0f, 20.0f, 21.0f, 22.0f },
};
static ALIGN16 const ScePspFVector4 src[2] = {
	{ 2.5f, -3.5f, 0.25f, -0.75f },
	{ 4.5f, -5.5f, 0.5f, -1.5f },
};

struct Result {
	ScePspFVector4 m[4];
	unsigned int spfx, tpfx, dpfx;
};

#define LOAD_TEST(name, dpfx, ops) \
	static void __attribute__((noinline)) name(struct Result *r) { \
		asm volatile ( \
			"la     $t5, pattern\n" \
			"lv.q   C000, 0($t5)\n" \
			"lv.q   C010, 16($t5)\n" \
			"lv.q   C020, 32($t5)\n" \
			"lv.q   C030, 48($t5)\n" \
			"la     $t3, src\n" \
			"li     $t4, 0x40400000\n" \
			dpfx "\n" \
			ops \
			"mfvc   $t0, $128\n" \
			"mfvc   $t1, $129\n" \
			"mfvc   $t2, $130\n" \
			"vmov.q C700, C700\n" \
			"sv.q   C000, 0+%0\n" \
			"sv.q   C010, 16+%0\n" \
			"sv.q   C020, 32+%0\n" \
			"sv.q   C030, 48+%0\n" \
			"sw     $t0, 64+%0\n" \
			"sw     $t1, 68+%0\n" \
			"sw     $t2, 72+%0\n" \
			: "+m" (*r) : : "t0", "t1", "t2", "t3", "t4", "t5", "memory" \
		); \
	}

#define D_MASK13 "vpfxd 0:1, m, -1:1, m"
#define D_MASK0 "vpfxd m, 0:1, m, 0:1"

LOAD_TEST(t_none,    D_MASK13, "")
LOAD_TEST(t_lv_q,    D_MASK13, "lv.q   C000, 0($t3)\n")
LOAD_TEST(t_lv_q_r,  D_MASK13, "lv.q   R000, 0($t3)\n")
LOAD_TEST(t_lv_s0,   D_MASK13, "lv.s   S000, 0($t3)\n")
LOAD_TEST(t_lv_s0m,  D_MASK0,  "lv.s   S000, 0($t3)\n")
LOAD_TEST(t_lv_s1,   D_MASK13, "lv.s   S010, 4($t3)\n")
LOAD_TEST(t_lv_s1m,  D_MASK0,  "lv.s   S010, 4($t3)\n")
LOAD_TEST(t_lvl_q,   D_MASK13, "lvl.q  C000, 24($t3)\n")
LOAD_TEST(t_lvr_q,   D_MASK13, "lvr.q  C000, 4($t3)\n")
LOAD_TEST(t_ulv_q,   D_MASK13, "lvl.q  C000, 16($t3)\nlvr.q  C000, 4($t3)\n")
LOAD_TEST(t_lv_q2,   D_MASK13, "lv.q   C000, 0($t3)\nlv.q   C010, 16($t3)\n")
LOAD_TEST(t_mtv,     D_MASK13, "mtv    $t4, S000\n")
LOAD_TEST(t_mtvm,    D_MASK0,  "mtv    $t4, S000\n")
LOAD_TEST(t_mtv1,    D_MASK0,  "mtv    $t4, S010\n")

static const struct {
	const char *name;
	void (*func)(struct Result *r);
} tests[] = {
	{ "(nothing)", t_none },
	{ "lv.q C", t_lv_q },
	{ "lv.q R", t_lv_q_r },
	{ "lv.s S000", t_lv_s0 },
	{ "lv.s S000 m0", t_lv_s0m },
	{ "lv.s S010", t_lv_s1 },
	{ "lv.s S010 m0", t_lv_s1m },
	{ "lvl.q", t_lvl_q },
	{ "lvr.q", t_lvr_q },
	{ "lvl.q+lvr.q", t_ulv_q },
	{ "lv.q x2", t_lv_q2 },
	{ "mtv S000", t_mtv },
	{ "mtv S000 m0", t_mtvm },
	{ "mtv S010 m0", t_mtv1 },
};

int main(int argc, char **argv) {
	for (size_t i = 0; i < ARRAY_SIZE(tests); i++) {
		struct Result r;
		memset(&r, 0, sizeof(r));
		tests[i].func(&r);
		printf("%-14s", tests[i].name);
		for (int c = 0; c < 4; c++) {
			const unsigned int *u = (const unsigned int *)&r.m[c];
			printf(" %08x,%08x,%08x,%08x", u[0], u[1], u[2], u[3]);
		}
		printf(" pfx=%08x,%08x,%08x\n", r.spfx, r.tpfx, r.dpfx);
	}
	return 0;
}
