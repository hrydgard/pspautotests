#include <common.h>
#include "vfpu_common.h"
#include <string.h>

// The VFPU random number generator: vrnds seeds it, vrndi draws an integer, vrndf1 a float in
// [1, 2), vrndf2 a float in [2, 4). The state lives in the control registers RCX0-RCX7 ($136 to
// $143), readable and writable with mfvc/mtvc. This records enough of the sequence for a few seeds
// to pin the generator down, the state after each draw, the two float formats, what a .p/.t/.q
// draw does, and what happens when the state is written directly.

typedef struct { unsigned int x, y, z, w; } UVec4;

static void __attribute__((noinline)) seed(unsigned int s) {
	asm volatile (
		".set noreorder\n"
		"mtv    %0, S000\n"
		"vrnds.s S000\n"
		"nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
		".set reorder\n"
		: : "r" (s) : "memory"
	);
}

static void __attribute__((noinline)) read_state(unsigned int *rcx) {
	asm volatile (
		".set noreorder\n"
		"nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
		"mfvc  $t0, $136\n" "sw $t0, 0(%0)\n"
		"mfvc  $t0, $137\n" "sw $t0, 4(%0)\n"
		"mfvc  $t0, $138\n" "sw $t0, 8(%0)\n"
		"mfvc  $t0, $139\n" "sw $t0, 12(%0)\n"
		"mfvc  $t0, $140\n" "sw $t0, 16(%0)\n"
		"mfvc  $t0, $141\n" "sw $t0, 20(%0)\n"
		"mfvc  $t0, $142\n" "sw $t0, 24(%0)\n"
		"mfvc  $t0, $143\n" "sw $t0, 28(%0)\n"
		".set reorder\n"
		: : "r" (rcx) : "t0", "memory"
	);
}

static void __attribute__((noinline)) write_state(const unsigned int *rcx) {
	asm volatile (
		".set noreorder\n"
		"lw $t0, 0(%0)\n"  "mtvc $t0, $136\n"
		"lw $t0, 4(%0)\n"  "mtvc $t0, $137\n"
		"lw $t0, 8(%0)\n"  "mtvc $t0, $138\n"
		"lw $t0, 12(%0)\n" "mtvc $t0, $139\n"
		"lw $t0, 16(%0)\n" "mtvc $t0, $140\n"
		"lw $t0, 20(%0)\n" "mtvc $t0, $141\n"
		"lw $t0, 24(%0)\n" "mtvc $t0, $142\n"
		"lw $t0, 28(%0)\n" "mtvc $t0, $143\n"
		"nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
		".set reorder\n"
		: : "r" (rcx) : "t0", "memory"
	);
}

static unsigned int __attribute__((noinline)) rndi_s(void) {
	unsigned int v;
	asm volatile (".set noreorder\nvrndi.s S000\nnop\nnop\nnop\nnop\nmfv %0, S000\n.set reorder\n" : "=r" (v) : : "memory");
	return v;
}
static unsigned int __attribute__((noinline)) rndf1_s(void) {
	unsigned int v;
	asm volatile (".set noreorder\nvrndf1.s S000\nnop\nnop\nnop\nnop\nmfv %0, S000\n.set reorder\n" : "=r" (v) : : "memory");
	return v;
}
static unsigned int __attribute__((noinline)) rndf2_s(void) {
	unsigned int v;
	asm volatile (".set noreorder\nvrndf2.s S000\nnop\nnop\nnop\nnop\nmfv %0, S000\n.set reorder\n" : "=r" (v) : : "memory");
	return v;
}
static void __attribute__((noinline)) rndi_p(UVec4 *out) {
	asm volatile (".set noreorder\nvmov.q C000, C000[0,0,0,0]\nvrndi.p C000\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (*out) : : "memory");
}
static void __attribute__((noinline)) rndi_t(UVec4 *out) {
	asm volatile (".set noreorder\nvmov.q C000, C000[0,0,0,0]\nvrndi.t C000\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (*out) : : "memory");
}
static void __attribute__((noinline)) rndi_q(UVec4 *out) {
	asm volatile (".set noreorder\nvrndi.q C000\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (*out) : : "memory");
}
static void __attribute__((noinline)) rndf1_q(UVec4 *out) {
	asm volatile (".set noreorder\nvrndf1.q C000\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (*out) : : "memory");
}
// vrndi with a prefix pending.
static void __attribute__((noinline)) rndi_q_prefixed(UVec4 *out) {
	asm volatile (".set noreorder\nvpfxs -x, |y|, 1, 0\nvpfxd 0:1, 0:1, m, -1:1\nvrndi.q C000\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (*out) : : "memory");
}

static void print_state(const char *label) {
	unsigned int rcx[8];
	read_state(rcx);
	printf("%s RCX: %08x %08x %08x %08x %08x %08x %08x %08x\n", label, rcx[0], rcx[1], rcx[2], rcx[3], rcx[4], rcx[5], rcx[6], rcx[7]);
}

int main(int argc, char *argv[]) {
	// The first VFPU instruction goes through the lazy coprocessor enable.
	asm volatile ("vnop\nvnop\nvnop\n" : : : "memory");

	// Whatever the state is when we get here, it's not ours (PSPLink and the kernel run VFPU code
	// too), so don't print it. Seed first.
	static const unsigned int seeds[] = { 0, 1, 0x12345678, 0xFFFFFFFF, 0x80000000, 0xDEADBEEF };
	for (int s = 0; s < ARRAY_SIZE(seeds); s++) {
		printf("-- seed %08x --\n", seeds[s]);
		seed(seeds[s]);
		print_state("after vrnds");
		for (int i = 0; i < 6; i++) {
			unsigned int v = rndi_s();
			char label[32];
			snprintf(label, sizeof(label), "vrndi=%08x", v);
			print_state(label);
		}
		// A longer run, values only, to catch anything periodic or carry-related.
		printf("next 40:");
		for (int i = 0; i < 40; i++) {
			printf(" %08x", rndi_s());
			if (i % 8 == 7 && i != 39) printf("\n        ");
		}
		printf("\n");
	}

	printf("-- float forms, seed 1 --\n");
	seed(1);
	for (int i = 0; i < 6; i++) {
		unsigned int f1 = rndf1_s();
		unsigned int f2 = rndf2_s();
		printf("vrndf1=%08x vrndf2=%08x\n", f1, f2);
	}
	print_state("after those");

	printf("-- vector draws, seed 1 --\n");
	seed(1);
	ALIGN16 UVec4 v;
	rndi_q(&v); printf("vrndi.q: %08x %08x %08x %08x\n", v.x, v.y, v.z, v.w); print_state("  state");
	rndf1_q(&v); printf("vrndf1.q: %08x %08x %08x %08x\n", v.x, v.y, v.z, v.w);
	rndi_q_prefixed(&v); printf("vrndi.q s[-x,|y|,1,0] d[0:1,0:1,m,-1:1]: %08x %08x %08x %08x\n", v.x, v.y, v.z, v.w);
	print_state("  state");

	printf("-- state written directly --\n");
	static const unsigned int st1[8] = { 0x00000001, 0x00000002, 0x00000004, 0x00000008, 0, 0, 0, 0 };
	static const unsigned int st2[8] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
	static const unsigned int st3[8] = { 0x3F80ABCD, 0x3F810000, 0x3F82FFFF, 0x3F830001, 0x3F840002, 0x3F850003, 0x3F860004, 0x3F870005 };
	static const unsigned int st4[8] = { 0x12345678, 0x9ABCDEF0, 0x0F0F0F0F, 0xF0F0F0F0, 0x00FF00FF, 0xFF00FF00, 0x55AA55AA, 0xAA55AA55 };
	const unsigned int *states[] = { st1, st2, st3, st4 };
	for (int s = 0; s < 4; s++) {
		write_state(states[s]);
		printf("wrote %08x %08x %08x %08x %08x %08x %08x %08x\n", states[s][0], states[s][1], states[s][2], states[s][3], states[s][4], states[s][5], states[s][6], states[s][7]);
		print_state("  reads back");
		for (int i = 0; i < 4; i++) {
			unsigned int val = rndi_s();
			char label[32];
			snprintf(label, sizeof(label), "  vrndi=%08x", val);
			print_state(label);
		}
	}
	// Last, since one of these hung the PSP the first time round: the pair and triple forms.
	printf("-- vrndi.p and vrndi.t --\n");
	seed(1);
	rndi_p(&v); printf("vrndi.p: %08x %08x %08x %08x\n", v.x, v.y, v.z, v.w); print_state("  state");
	rndi_t(&v); printf("vrndi.t: %08x %08x %08x %08x\n", v.x, v.y, v.z, v.w); print_state("  state");
	return 0;
}
