#include <common.h>
#include "vfpu_common.h"
#include <string.h>
#include <math.h>

// A JIT stress test, not a hardware behaviour test: on hardware there is nothing here that could
// go wrong. Emulator JITs implement vsin/vcos by calling a host function, and this checks that
// they save every host register they had the guest's FPU state in before doing so. The block in
// callout_asm.S keeps 24 FPU registers live and dirty across the call, enough to have spilled
// into the host's caller-saved registers on any backend.

void callout_fpu(const float *in24, const float *angles4, float *out24, float *sincos8);

int main(int argc, char *argv[]) {
	ALIGN16 float in[24];
	ALIGN16 float out[24];
	ALIGN16 float angles[4] = { 0.0f, 0.5f, 1.0f, 1.5f };  // in units of pi/2, as vsin takes them
	ALIGN16 float sincos[8];

	for (int i = 0; i < 24; i++) {
		in[i] = 1.0f + i * 0.25f;
		out[i] = -999.0f;
	}
	memset(sincos, 0, sizeof(sincos));

	callout_fpu(in, angles, out, sincos);

	int bad = 0;
	for (int i = 0; i < 24; i++) {
		if (out[i] != in[i] + in[i]) {
			printf("f%d: expected %f, got %f\n", i, in[i] + in[i], out[i]);
			bad++;
		}
	}
	printf("FPU registers survived the call-out: %s\n", bad == 0 ? "yes" : "NO");
	for (int i = 0; i < 4; i++) {
		printf("sin(%g): %.4f  cos(%g): %.4f\n", angles[i], sincos[i], angles[i], sincos[4 + i]);
	}
	return 0;
}
