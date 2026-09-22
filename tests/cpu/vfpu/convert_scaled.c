#include <common.h>
#include "vfpu_common.h"
#include <string.h>
#include <math.h>

// vf2i*/vi2f with a non-zero scale, which convert.c never uses. The scale multiplies (or
// divides) by 2^n before converting, and the interesting inputs are the ones the multiply
// can't make ordinary: NaN, infinities, values that saturate, and values so small that the
// scale is what makes them representable.

typedef struct { unsigned int x, y, z, w; } UVec4;

#define CONVERT(name, instr, scale) \
	static void __attribute__((noinline)) name(ScePspFVector4 *out, const ScePspFVector4 *in) { \
		asm volatile ( \
			"lv.q   C100, %1\n" \
			instr ".q C200, C100, " #scale "\n" \
			"sv.q   C200, %0\n" \
			: "+m" (*out) : "m" (*in) \
		); \
	}

CONVERT(vf2iz_1, "vf2iz", 1)
CONVERT(vf2iz_5, "vf2iz", 5)
CONVERT(vf2iz_16, "vf2iz", 16)
CONVERT(vf2iz_31, "vf2iz", 31)
CONVERT(vf2in_1, "vf2in", 1)
CONVERT(vf2in_5, "vf2in", 5)
CONVERT(vf2in_16, "vf2in", 16)
CONVERT(vf2in_31, "vf2in", 31)
CONVERT(vf2id_5, "vf2id", 5)
CONVERT(vf2id_31, "vf2id", 31)
CONVERT(vf2iu_5, "vf2iu", 5)
CONVERT(vf2iu_31, "vf2iu", 31)
CONVERT(vi2f_1, "vi2f", 1)
CONVERT(vi2f_5, "vi2f", 5)
CONVERT(vi2f_16, "vi2f", 16)
CONVERT(vi2f_31, "vi2f", 31)

typedef void (*ConvertFunc)(ScePspFVector4 *, const ScePspFVector4 *);

static const struct { const char *name; ConvertFunc func; } toInt[] = {
	{ "vf2iz 1", vf2iz_1 }, { "vf2iz 5", vf2iz_5 }, { "vf2iz 16", vf2iz_16 }, { "vf2iz 31", vf2iz_31 },
	{ "vf2in 1", vf2in_1 }, { "vf2in 5", vf2in_5 }, { "vf2in 16", vf2in_16 }, { "vf2in 31", vf2in_31 },
	{ "vf2id 5", vf2id_5 }, { "vf2id 31", vf2id_31 },
	{ "vf2iu 5", vf2iu_5 }, { "vf2iu 31", vf2iu_31 },
};

static const struct { const char *name; ConvertFunc func; } toFloat[] = {
	{ "vi2f 1", vi2f_1 }, { "vi2f 5", vi2f_5 }, { "vi2f 16", vi2f_16 }, { "vi2f 31", vi2f_31 },
};

static ALIGN16 const ScePspFVector4 floatInputs[] = {
	{ NAN, -NAN, INFINITY, -INFINITY },
	{ 0.0f, -0.0f, 1.0f, -1.0f },
	{ 1.5f, -1.5f, 2.5f, -2.5f },
	{ 0.3f, -0.3f, 0.7f, -0.7f },
	{ 1e-6f, -1e-6f, 1e-9f, -1e-9f },  // only representable once scaled up
	{ 1000.0f, -1000.0f, 65535.5f, -65535.5f },
	{ 3e9f, -3e9f, 2147483648.0f, -2147483648.0f },  // saturate at most scales
	{ 2147483520.0f, -2147483520.0f, 1e30f, -1e30f },  // largest float below INT_MAX, and huge
};

static ALIGN16 const UVec4 intInputs[] = {
	{ 0, 1, 0xFFFFFFFF, 2 },
	{ 0x7FFFFFFF, 0x80000000, 0x40000000, 0xC0000000 },
	{ 100, 0xFFFFFF9C, 0x00010000, 0xFFFF0000 },
	{ 0x12345678, 0xEDCBA988, 0x00000003, 0xFFFFFFFD },
};

int main(int argc, char *argv[]) {
	ALIGN16 ScePspFVector4 out;
	UVec4 bits;

	for (int f = 0; f < ARRAY_SIZE(toInt); f++) {
		printf("-- %s --\n", toInt[f].name);
		for (int i = 0; i < ARRAY_SIZE(floatInputs); i++) {
			toInt[f].func(&out, &floatInputs[i]);
			memcpy(&bits, &out, sizeof(bits));
			printf("%g,%g,%g,%g: %08x,%08x,%08x,%08x\n",
				floatInputs[i].x, floatInputs[i].y, floatInputs[i].z, floatInputs[i].w,
				bits.x, bits.y, bits.z, bits.w);
		}
	}

	for (int f = 0; f < ARRAY_SIZE(toFloat); f++) {
		printf("-- %s --\n", toFloat[f].name);
		for (int i = 0; i < ARRAY_SIZE(intInputs); i++) {
			toFloat[f].func(&out, (const ScePspFVector4 *)&intInputs[i]);
			memcpy(&bits, &out, sizeof(bits));
			printf("%08x,%08x,%08x,%08x: %08x,%08x,%08x,%08x (%g,%g,%g,%g)\n",
				intInputs[i].x, intInputs[i].y, intInputs[i].z, intInputs[i].w,
				bits.x, bits.y, bits.z, bits.w, out.x, out.y, out.z, out.w);
		}
	}
	return 0;
}
