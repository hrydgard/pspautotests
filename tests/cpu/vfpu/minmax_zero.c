#include <common.h>
#include "vfpu_common.h"
#include <string.h>
#include <math.h>

// vmin/vmax on the inputs that don't compare like ordinary numbers: the two zeros, which are
// equal but distinguishable, and denormals, which most VFPU operations flush to zero. Kept apart
// from minmax.c since an emulator may not care to get these right.

#define MINMAX(name, instr) \
	static void __attribute__((noinline)) name(ScePspFVector4 *out, const ScePspFVector4 *a, const ScePspFVector4 *b) { \
		asm volatile ( \
			"lv.q   C100, %1\n" \
			"lv.q   C110, %2\n" \
			instr ".q C200, C100, C110\n" \
			"sv.q   C200, %0\n" \
			: "+m" (*out) : "m" (*a), "m" (*b) \
		); \
	}

MINMAX(vmin_q, "vmin")
MINMAX(vmax_q, "vmax")

typedef struct { unsigned int x, y, z, w; } UVec4;

static ALIGN16 const ScePspFVector4 pairs[][2] = {
	{ { -0.0f, 0.0f, -0.0f, 0.0f }, { 0.0f, -0.0f, -0.0f, 0.0f } },
	{ { -0.0f, 0.0f, -0.0f, 0.0f }, { 1.0f, 1.0f, -1.0f, -1.0f } },
	{ { 1e-40f, -1e-40f, 1e-40f, -1e-40f }, { 0.0f, 0.0f, -0.0f, -0.0f } },
	{ { 1e-40f, -1e-40f, 1e-40f, -1e-40f }, { 2e-40f, -2e-40f, -2e-40f, 2e-40f } },
	{ { 1e-40f, -1e-40f, 1e-40f, -1e-40f }, { 1.0f, 1.0f, -1.0f, -1.0f } },
	{ { 1e-40f, -1e-40f, NAN, -NAN }, { NAN, -NAN, 1e-40f, -1e-40f } },
};

int main(int argc, char *argv[]) {
	ALIGN16 ScePspFVector4 out;
	UVec4 a, b, r;

	for (int i = 0; i < ARRAY_SIZE(pairs); i++) {
		memcpy(&a, &pairs[i][0], sizeof(a));
		memcpy(&b, &pairs[i][1], sizeof(b));
		vmin_q(&out, &pairs[i][0], &pairs[i][1]);
		memcpy(&r, &out, sizeof(r));
		printf("vmin %08x,%08x,%08x,%08x  %08x,%08x,%08x,%08x: %08x,%08x,%08x,%08x\n",
			a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w, r.x, r.y, r.z, r.w);
		vmax_q(&out, &pairs[i][0], &pairs[i][1]);
		memcpy(&r, &out, sizeof(r));
		printf("vmax %08x,%08x,%08x,%08x  %08x,%08x,%08x,%08x: %08x,%08x,%08x,%08x\n",
			a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w, r.x, r.y, r.z, r.w);
	}
	return 0;
}
