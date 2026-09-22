#include <common.h>
#include "vfpu_common.h"
#include <string.h>
#include <math.h>

// Mostly a JIT stress test: the NaN cases for vmin/vmax are the only part that tests hardware
// behaviour vector.c doesn't (signed zero and denormals are in minmax_zero.c, since an emulator
// may reasonably not bother with those). The rest is for emulator JITs, which need a
// scratch integer register for the NaN handling in these and have to spill one to get it when
// this many are live (minmax_asm.S keeps 21 live and dirty). A spill that only happens on one
// of the two paths through the min/max leaves the wrong value in the register on the other.

void minmax_pressure(const int *in21, const float *pairs, int *out21, float *results);

typedef struct { unsigned int x, y, z, w; } UVec4;

static ALIGN16 const float pairs[16][4] = {
	// a                              b
	{ 1.0f, 2.0f, -1.0f, -2.0f },    { 2.0f, 1.0f, -2.0f, -1.0f },   // plain
	{ NAN, NAN, NAN, NAN },          { 1.0f, -1.0f, 0.0f, INFINITY },  // NaN in a
	{ 1.0f, -1.0f, 0.0f, INFINITY }, { NAN, NAN, NAN, NAN },           // NaN in b
	{ NAN, -NAN, NAN, -NAN },        { NAN, NAN, -NAN, -NAN },         // NaN in both
	{ -1.0f, -3.0f, -4.0f, -1e30f }, { -2.0f, -2.5f, -3.0f, -1e-30f },  // both negative
	{ INFINITY, -INFINITY, 0.0f, 5.0f }, { -INFINITY, INFINITY, 1.0f, 5.0f },
	{ 1e-30f, -1e-30f, 1.5f, 2.5f }, { 0.0f, 0.0f, 1.5f, 2.5f },       // tiny, equal
	{ NAN, 3.0f, -NAN, -3.0f },      { 3.0f, NAN, -3.0f, -NAN },       // NaN scattered
};

int main(int argc, char *argv[]) {
	int in[21], out[21];
	ALIGN16 float results[16][4];
	UVec4 bits;

	for (int i = 0; i < 21; i++) {
		in[i] = 0x1000 * (i + 1);
		out[i] = -1;
	}
	memset(results, 0, sizeof(results));

	minmax_pressure(in, &pairs[0][0], out, &results[0][0]);

	int bad = 0;
	for (int i = 0; i < 21; i++) {
		if (out[i] != in[i] + 1) {
			printf("reg %d: expected %08x, got %08x\n", i, in[i] + 1, out[i]);
			bad++;
		}
	}
	printf("GPRs survived: %s\n", bad == 0 ? "yes" : "NO");

	for (int p = 0; p < 8; p++) {
		memcpy(&bits, results[p], sizeof(bits));
		printf("vmin %d: %08x,%08x,%08x,%08x\n", p, bits.x, bits.y, bits.z, bits.w);
	}
	for (int p = 0; p < 8; p++) {
		memcpy(&bits, results[8 + p], sizeof(bits));
		printf("vmax %d: %08x,%08x,%08x,%08x\n", p, bits.x, bits.y, bits.z, bits.w);
	}
	return 0;
}
