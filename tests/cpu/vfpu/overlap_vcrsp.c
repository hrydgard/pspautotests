#include <common.h>
#include "vfpu_common.h"
#include <string.h>

// vcrsp with the destination overlapping a source. The assembler refuses this ("destination
// register conflict"), so it's assembled as raw words; a compiled game can't contain it, which is
// why it's kept out of overlap.c. The hardware doesn't read all its inputs first here: lanes y and
// z come out right, but x is computed from the register file after they've been written, so with
// s = (1,2,3), t = (101,102,103) and d one row down, d.x sees s.z = 200 (the new d.y) and gives
// 206 - 200*102 instead of 206 - 306.

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

CASE(vcrsp_dds,    ".word 0xf2848000\n")          // vcrsp.t C000, C000, C100
CASE(vcrsp_ddt,    ".word 0xf2800400\n")          // vcrsp.t C000, C100, C000
CASE(vcrsp_shift,  ".word 0xf2848040\n")          // vcrsp.t C001, C000, C100
CASE(vcrsp_shift2, ".word 0xf284c000\n")          // vcrsp.t C000, C001, C100

typedef void (*CaseFunc)(Mat *);
#define ENTRY(f, text) { #f, text, f }
static const struct { const char *name; const char *text; CaseFunc func; } cases[] = {
	ENTRY(vcrsp_dds, "vcrsp.t C000, C000, C100"), ENTRY(vcrsp_ddt, "vcrsp.t C000, C100, C000"),
	ENTRY(vcrsp_shift, "vcrsp.t C001, C000, C100"), ENTRY(vcrsp_shift2, "vcrsp.t C000, C001, C100"),
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
