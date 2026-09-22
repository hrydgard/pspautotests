#include <common.h>
#include <string.h>

// The rounding mode is the low two bits of fcr31, and the bits above it (flags at 2-6, cause
// at 12-17, FS at 24) must not leak into it. A JIT that hands more than the mode to the host's
// rounding control gets a different mode whenever a flag happens to be set - which after any
// inexact result is most of the time.
//
// The enable bits at 7-11 are left alone: with one set, the first inexact result traps, and
// the test hangs on hardware. So is cause bit 17 (unimplemented operation), which has no enable
// and traps as soon as it's written.

static const unsigned int extraBits[] = {
	0x00000000,  // nothing
	0x00000004,  // inexact flag
	0x0000007C,  // all flags
	0x0001F000,  // the cause bits that have enables
	0x01000000,  // FS
	0x0101F07C,  // all of the above
};

static const char *extraNames[] = {
	"clean", "I flag", "all flags", "cause", "FS", "all of the above",
};

static const float values[] = {
	0.5f, 1.5f, 2.5f, -0.5f, -1.5f, -2.5f, 0.49999997f, 1.0000001f, 3.0000002f, -3.9999998f,
};

// Only the result. What the flags look like afterwards is fcr.c's business.
__attribute__((noinline)) static int cvtws(float x, unsigned int fcr31) {
	float resultFloat;
	int result;
	asm volatile (
		"ctc1 %2, $31\n"
		"cvt.w.s %0, %1\n"
		"ctc1 $0, $31\n"
		: "=f"(resultFloat) : "f"(x), "r"(fcr31)
	);
	memcpy(&result, &resultFloat, sizeof(result));
	return result;
}

// An add whose result depends on the rounding mode: 1 + 2^-24 is exactly halfway between two
// floats, and 1 + 2^-25 is below halfway.
__attribute__((noinline)) static float addRounded(float a, float b, unsigned int fcr31) {
	float result;
	asm volatile (
		"ctc1 %3, $31\n"
		"add.s %0, %1, %2\n"
		"ctc1 $0, $31\n"
		: "=f"(result) : "f"(a), "f"(b), "r"(fcr31)
	);
	return result;
}

int main(int argc, char *argv[]) {
	for (int rm = 0; rm < 4; rm++) {
		for (int e = 0; e < ARRAY_SIZE(extraBits); e++) {
			unsigned int fcr31 = rm | extraBits[e];
			printf("-- rm=%d %s (fcr31=%08x) --\n", rm, extraNames[e], fcr31);
			for (int i = 0; i < ARRAY_SIZE(values); i++) {
				printf("cvt.w.s %f: %d\n", values[i], cvtws(values[i], fcr31));
			}
			unsigned int oneRaw = 0x3F800000;
			float one;
			memcpy(&one, &oneRaw, sizeof(one));
			unsigned int halfUlpRaw = 0x33800000;  // 2^-24
			float halfUlp;
			memcpy(&halfUlp, &halfUlpRaw, sizeof(halfUlp));
			float r;
			unsigned int rRaw;
			r = addRounded(one, halfUlp, fcr31);
			memcpy(&rRaw, &r, sizeof(rRaw));
			printf("1 + 2^-24: %08x\n", rRaw);
			r = addRounded(-one, -halfUlp, fcr31);
			memcpy(&rRaw, &r, sizeof(rRaw));
			printf("-1 - 2^-24: %08x\n", rRaw);
			r = addRounded(one, halfUlp / 2.0f, fcr31);
			memcpy(&rRaw, &r, sizeof(rRaw));
			printf("1 + 2^-25: %08x\n", rRaw);
		}
	}
	return 0;
}
