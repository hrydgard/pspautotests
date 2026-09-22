#include <common.h>
#include "vfpu_common.h"
#include <string.h>

// A prefix set in one place and consumed somewhere else: across a taken branch, a not-taken
// branch, a jump, a call and return, a delay slot, and a loop iteration. On hardware the prefix
// is just a register that stays put. An emulator that tracks prefixes at compile time has to
// notice when it can't see the consumer.

typedef struct { unsigned int x, y, z, w; } UVec4;

static ALIGN16 const ScePspFVector4 inS = { 1.5f, -2.5f, 3.5f, -4.5f };
static ALIGN16 const ScePspFVector4 inT = { 10.0f, 20.0f, 30.0f, 40.0f };

#define SETUP \
	"lv.q   C100, %1\n" \
	"lv.q   C110, %2\n" \
	"vpfxs  -y, -x, -w, -z\n" \
	"vpfxt  1, 1/2, 3, 0\n" \
	"vpfxd  0:1, 0:1, 0:1, 0:1\n"

#define FINISH \
	"sv.q   C000, %0\n"

static void __attribute__((noinline)) taken_branch(ScePspFVector4 *out, int flag) {
	asm volatile (
		SETUP
		"beqz   %3, 1f\n"
		"nop\n"
		"vadd.q C000, C100, C110\n"   // not reached
		"j      2f\n"
		"nop\n"
		"1:\n"
		"vadd.q C000, C100, C110\n"   // the consumer, in another block
		"2:\n"
		FINISH
		: "+m" (*out) : "m" (inS), "m" (inT), "r" (flag) : "memory"
	);
}

static void __attribute__((noinline)) delay_slot(ScePspFVector4 *out, int flag) {
	asm volatile (
		SETUP
		"beqz   %3, 1f\n"
		"vadd.q C000, C100, C110\n"   // the consumer, in the delay slot
		"nop\n"
		"1:\n"
		FINISH
		: "+m" (*out) : "m" (inS), "m" (inT), "r" (flag) : "memory"
	);
}

static void __attribute__((noinline)) prefix_in_delay_slot(ScePspFVector4 *out, int flag) {
	asm volatile (
		"lv.q   C100, %1\n"
		"lv.q   C110, %2\n"
		"vpfxt  1, 1/2, 3, 0\n"
		"vpfxd  0:1, 0:1, 0:1, 0:1\n"
		"beqz   %3, 1f\n"
		"vpfxs  -y, -x, -w, -z\n"    // the prefix itself in the delay slot
		"nop\n"
		"1:\n"
		"vadd.q C000, C100, C110\n"
		FINISH
		: "+m" (*out) : "m" (inS), "m" (inT), "r" (flag) : "memory"
	);
}

// Plain C functions, so the compiler's own prologue and epilogue (ordinary MIPS code) sit between
// the prefix and the consumer too.
void __attribute__((noinline)) consumer(void) {
	asm volatile ("vadd.q C000, C100, C110\n" : : : "memory");
}

static void __attribute__((noinline)) across_call(ScePspFVector4 *out) {
	asm volatile (
		"addiu  $sp, $sp, -16\n"
		"sw     $ra, 0($sp)\n"
		SETUP
		"jal    consumer\n"           // the consumer is in another function
		"nop\n"
		"lw     $ra, 0($sp)\n"
		"addiu  $sp, $sp, 16\n"
		FINISH
		: "+m" (*out) : "m" (inS), "m" (inT) : "memory"
	);
}

void __attribute__((noinline)) set_prefixes(void) {
	asm volatile (
		"vpfxs  -y, -x, -w, -z\n"
		"vpfxt  1, 1/2, 3, 0\n"
		"vpfxd  0:1, 0:1, 0:1, 0:1\n"
		: : : "memory"
	);
}

static void __attribute__((noinline)) from_call(ScePspFVector4 *out) {
	asm volatile (
		"addiu  $sp, $sp, -16\n"
		"sw     $ra, 0($sp)\n"
		"lv.q   C100, %1\n"
		"lv.q   C110, %2\n"
		"jal    set_prefixes\n"       // the prefixes set in another function
		"nop\n"
		"lw     $ra, 0($sp)\n"
		"addiu  $sp, $sp, 16\n"
		"vadd.q C000, C100, C110\n"
		FINISH
		: "+m" (*out) : "m" (inS), "m" (inT) : "memory"
	);
}

static void __attribute__((noinline)) loop(ScePspFVector4 *out) {
	// Iteration 1 sets the prefixes and consumes them; iteration 2 sets only S, so the vadd's
	// T and D must be back to default. What C000 holds after is what iteration 2 computed.
	asm volatile (
		"lv.q   C100, %1\n"
		"lv.q   C110, %2\n"
		"li     $t0, 2\n"
		"vpfxt  1, 1/2, 3, 0\n"
		"vpfxd  0:1, 0:1, 0:1, 0:1\n"
		"1:\n"
		"vpfxs  -y, -x, -w, -z\n"
		"vadd.q C000, C100, C110\n"
		"addiu  $t0, $t0, -1\n"
		"bnez   $t0, 1b\n"
		"nop\n"
		FINISH
		: "+m" (*out) : "m" (inS), "m" (inT) : "t0", "memory"
	);
}

static void __attribute__((noinline)) syscall_between(ScePspFVector4 *out) {
	// A syscall (a function call into the kernel) between the prefix and its consumer.
	asm volatile (
		SETUP
		: : "m" (*out), "m" (inS), "m" (inT) : "memory"
	);
	sceKernelDelayThread(1);
	asm volatile (
		"vadd.q C000, C100, C110\n"
		FINISH
		: "+m" (*out) : : "memory"
	);
}

static void show(const char *label, const ScePspFVector4 *v) {
	UVec4 u;
	memcpy(&u, v, 16);
	printf("%-22s %08x,%08x,%08x,%08x\n", label, u.x, u.y, u.z, u.w);
}

int main(int argc, char *argv[]) {
	ALIGN16 ScePspFVector4 out;

	taken_branch(&out, 0);        show("taken branch", &out);
	taken_branch(&out, 1);        show("not taken branch", &out);
	delay_slot(&out, 0);          show("consumer in delay slot", &out);
	delay_slot(&out, 1);          show("consumer in delay slot 2", &out);
	prefix_in_delay_slot(&out, 0); show("prefix in delay slot", &out);
	prefix_in_delay_slot(&out, 1); show("prefix in delay slot 2", &out);
	across_call(&out);            show("consumer in callee", &out);
	from_call(&out);              show("prefix set in callee", &out);
	loop(&out);                   show("loop", &out);
	syscall_between(&out);        show("syscall between", &out);
	return 0;
}
