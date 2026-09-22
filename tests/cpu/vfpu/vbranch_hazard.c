#include <common.h>

// Pipeline hazards around the VFPU condition code: how many instructions after an mtvc to the CC
// register a bvt sees the new value (more than two; three is on the edge and varies, six is enough), how soon after a vcmp a bvt sees it (at once),
// and how soon after a vcmp an mfvc does (one instruction). These are what a compiler would pad
// for, so an emulator need not reproduce them; they're here so the padding in vbranch.c is
// explained, and so nobody wonders.

// How soon after mtvc (and after vcmp) does a branch see the new CC? n instructions between.
#define LATENCY_CASE(name, setup, pad) \
	static int __attribute__((noinline)) name(unsigned int cc, float a, float b) { \
		int taken; \
		asm volatile ( \
			".set noreorder\n" \
			"mtv    %2, S000\n" \
			"mtv    %3, S001\n" \
			"li     %0, 0\n" \
			setup \
			pad \
			"bvt    0, 1f\n" \
			"nop\n" \
			"j      2f\n" \
			"nop\n" \
			"1:\n" \
			"li     %0, 1\n" \
			"2:\n" \
			".set reorder\n" \
			: "=&r" (taken) : "r" (cc), "r" (a), "r" (b) : "memory" \
		); \
		return taken; \
	}
LATENCY_CASE(lat_mtvc0, "mtvc %1, $131\n", "")
LATENCY_CASE(lat_mtvc1, "mtvc %1, $131\n", "nop\n")
LATENCY_CASE(lat_mtvc2, "mtvc %1, $131\n", "nop\nnop\n")
LATENCY_CASE(lat_vcmp0, "vcmp.s EQ, S000, S001\n", "")
LATENCY_CASE(lat_vcmp1, "vcmp.s EQ, S000, S001\n", "nop\n")
LATENCY_CASE(lat_mtvc_vcmp, "mtvc %1, $131\nvcmp.s EQ, S000, S001\n", "")
LATENCY_CASE(lat_vcmp_mtvc, "vcmp.s EQ, S000, S001\nmtvc %1, $131\n", "")
LATENCY_CASE(lat_mtvc6, "mtvc %1, $131\n", "nop\nnop\nnop\nnop\nnop\nnop\n")

// And how soon after vcmp does mfvc see the new CC?
#define MFVC_LAT(name, pad) \
	static unsigned int __attribute__((noinline)) name(float a, float b) { \
		unsigned int v; \
		asm volatile (".set noreorder\n" "mtv %1, S000\nmtv %2, S001\nmtvc $zero, $131\nnop\nnop\nnop\nnop\nnop\nnop\nvcmp.s EQ, S000, S001\n" ".set reorder\n" pad "mfvc %0, $131\n" : "=&r" (v) : "r" (a), "r" (b) : "memory"); \
		return v; \
	}
MFVC_LAT(mfvc_lat0, "")
MFVC_LAT(mfvc_lat1, "nop\n")
MFVC_LAT(mfvc_lat3, "nop\nnop\nnop\n")


int main(int argc, char *argv[]) {
	// The first VFPU instruction in a process goes through the lazy coprocessor enable, which
	// throws the timing of everything around it. Get that over with.
	asm volatile (".set noreorder\n" "mtvc $zero, $131\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nvnop\nmfvc $t0, $131\n" ".set reorder\n" : : : "t0", "memory");

	printf("-- how soon after a CC write does bvt see it (previous CC 0, written 1 / previous 1, written 0) --\n");
	for (int prev = 0; prev < 2; prev++) {
		unsigned int before = prev ? 0x3F : 0x00, after = prev ? 0x00 : 0x3F;
		asm volatile (".set noreorder\n" "mtvc %0, $131\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n" ".set reorder\n" : : "r" (before) : "memory");
		printf("mtvc +0: %d  ", lat_mtvc0(after, 1.0f, 1.0f));
		asm volatile (".set noreorder\n" "mtvc %0, $131\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n" ".set reorder\n" : : "r" (before) : "memory");
		printf("+1: %d  ", lat_mtvc1(after, 1.0f, 1.0f));
		asm volatile (".set noreorder\n" "mtvc %0, $131\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n" ".set reorder\n" : : "r" (before) : "memory");
		printf("+2: %d  ", lat_mtvc2(after, 1.0f, 1.0f));
		asm volatile (".set noreorder\n" "mtvc %0, $131\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n" ".set reorder\n" : : "r" (before) : "memory");
		printf("+6: %d\n", lat_mtvc6(after, 1.0f, 1.0f));
		asm volatile (".set noreorder\n" "mtvc %0, $131\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n" ".set reorder\n" : : "r" (before) : "memory");
		printf("vcmp +0: %d  ", lat_vcmp0(0, 1.0f, prev ? 2.0f : 1.0f));
		asm volatile (".set noreorder\n" "mtvc %0, $131\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n" ".set reorder\n" : : "r" (before) : "memory");
		printf("+1: %d  ", lat_vcmp1(0, 1.0f, prev ? 2.0f : 1.0f));
		printf("mtvc(1) then vcmp(false): %d  ", lat_mtvc_vcmp(0x3F, 1.0f, 2.0f));
		printf("vcmp(true) then mtvc(0): %d\n", lat_vcmp_mtvc(0x00, 1.0f, 1.0f));
	}

	printf("mfvc after vcmp(true) +0: %02x  +1: %02x  +3: %02x\n", mfvc_lat0(1.0f, 1.0f), mfvc_lat1(1.0f, 1.0f), mfvc_lat3(1.0f, 1.0f));

	return 0;
}
