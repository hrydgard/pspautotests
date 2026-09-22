#include <common.h>
#include <string.h>
#include <pspthreadman.h>

// ll and sc: when does the conditional store go through, what does it write, and what does it
// leave in rt. Between the ll and the sc: nothing, an ordinary store to the same word, a store
// somewhere else, a load, a syscall, an interrupt and a thread switch. Also sc on a different
// word than the ll, two sc after one ll, and sc with no ll before it.
//
// What comes out: only ll sets the bit, any exception (syscall or interrupt) clears it, and sc
// itself leaves it alone. The address isn't checked at all. One thing this test avoids: an sc
// with no ll right after a printf succeeds, because the kernel's IO path runs an ll of its own
// and returns with the bit still set. That's a property of the kernel code a syscall happens to
// run through, not of the CPU, so it isn't recorded here.

static unsigned int __attribute__((aligned(16))) words[4];

// Runs ll on words[0], then the given code, then sc of value to words[scIndex]. Returns rt.
#define LLSC(name, between) \
	static int name(unsigned int value, int scIndex) { \
		unsigned int *p = words; \
		int rt = (int)value; \
		asm volatile ( \
			".set noreorder\n" \
			"ll   $t0, 0(%1)\n" \
			between \
			"sll  $t1, %2, 2\n" \
			"addu $t1, $t1, %1\n" \
			"sc   %0, 0($t1)\n" \
			"nop\n" \
			".set reorder\n" \
			: "+r" (rt) : "r" (p), "r" (scIndex) : "t0", "t1", "memory" \
		); \
		return rt; \
	}

LLSC(llsc_plain, "nop\n")
LLSC(llsc_store_same, "li $t1, 0x77\n" "sw $t1, 0(%1)\n")
LLSC(llsc_store_other, "li $t1, 0x77\n" "sw $t1, 4(%1)\n")
LLSC(llsc_load_same, "lw $t1, 0(%1)\n" "nop\n")
LLSC(llsc_ll_again, "ll $t1, 8(%1)\n" "nop\n")

// A syscall (no thread switch) and a delay (a switch, so the kernel's eret is in between).
static int llsc_syscall(unsigned int value) {
	unsigned int *p = words;
	int rt = (int)value;
	asm volatile ("ll $t0, 0(%0)" : : "r" (p) : "t0", "memory");
	sceKernelGetSystemTimeLow();
	asm volatile ("sc %0, 0(%1)" : "+r" (rt) : "r" (p) : "memory");
	return rt;
}

static int llsc_delay(unsigned int value) {
	unsigned int *p = words;
	int rt = (int)value;
	asm volatile ("ll $t0, 0(%0)" : : "r" (p) : "t0", "memory");
	sceKernelDelayThread(2000);
	asm volatile ("sc %0, 0(%1)" : "+r" (rt) : "r" (p) : "memory");
	return rt;
}

// sc with no ll (the previous sc, successful or not, is what's before it).
static int sc_alone(unsigned int value) {
	unsigned int *p = words;
	int rt = (int)value;
	asm volatile ("sc %0, 0(%1)" : "+r" (rt) : "r" (p) : "memory");
	return rt;
}

// Two sc in a row after one ll, with and without a syscall in between.
static void llsc_twice(int syscall, int *rt1, int *rt2) {
	unsigned int *p = words;
	int a = 0xAAAAAAAA, b = 0xBBBBBBBB;
	asm volatile ("ll $t0, 0(%0)" : : "r" (p) : "t0", "memory");
	if (syscall) {
		sceKernelGetSystemTimeLow();
	}
	asm volatile ("sc %0, 0(%1)" : "+r" (a) : "r" (p) : "memory");
	asm volatile ("sc %0, 4(%1)" : "+r" (b) : "r" (p) : "memory");
	*rt1 = a;
	*rt2 = b;
}

// A hardware interrupt but no syscall: spin long enough for a few vblanks.
static int llsc_spin(unsigned int value) {
	unsigned int *p = words;
	int rt = (int)value;
	asm volatile ("ll $t0, 0(%0)" : : "r" (p) : "t0", "memory");
	for (volatile int i = 0; i < 2000000; i++) {
	}
	asm volatile ("sc %0, 0(%1)" : "+r" (rt) : "r" (p) : "memory");
	return rt;
}

// ll, a syscall, then ll again before the sc.
static int llsc_rearm(unsigned int value) {
	unsigned int *p = words;
	int rt = (int)value;
	asm volatile ("ll $t0, 0(%0)" : : "r" (p) : "t0", "memory");
	sceKernelGetSystemTimeLow();
	asm volatile ("ll $t0, 0(%0)" : : "r" (p) : "t0", "memory");
	asm volatile ("sc %0, 0(%1)" : "+r" (rt) : "r" (p) : "memory");
	return rt;
}

// A syscall and then sc with no ll.
static int sc_after_syscall(unsigned int value) {
	unsigned int *p = words;
	int rt = (int)value;
	sceKernelGetSystemTimeLow();
	asm volatile ("sc %0, 0(%1)" : "+r" (rt) : "r" (p) : "memory");
	return rt;
}

static void reset(void) {
	words[0] = 0x11111111;
	words[1] = 0x22222222;
	words[2] = 0x33333333;
	words[3] = 0x44444444;
}

static void report(const char *label, int rt) {
	printf("%-28s rt=%d words=%08x %08x %08x %08x\n", label, rt, words[0], words[1], words[2], words[3]);
}

int main(int argc, char *argv[]) {
	int rt1, rt2;
	// Before anything else has run in this thread.
	reset(); report("sc first", sc_alone(0xAAAAAAAA));
	reset(); report("ll, sc", llsc_plain(0xAAAAAAAA, 0));
	reset(); report("ll, sw same, sc", llsc_store_same(0xAAAAAAAA, 0));
	reset(); report("ll, sw other, sc", llsc_store_other(0xAAAAAAAA, 0));
	reset(); report("ll, lw same, sc", llsc_load_same(0xAAAAAAAA, 0));
	reset(); report("ll, ll other, sc", llsc_ll_again(0xAAAAAAAA, 0));
	reset(); report("ll word 0, sc word 1", llsc_plain(0xAAAAAAAA, 1));
	reset(); report("ll word 0, sc word 3", llsc_plain(0xAAAAAAAA, 3));
	reset(); report("ll, syscall, sc", llsc_syscall(0xAAAAAAAA));
	reset(); report("ll, delay thread, sc", llsc_delay(0xAAAAAAAA));
	reset(); report("ll, sc (rt = 0)", llsc_plain(0, 0));
	reset(); report("ll, sc, then sc alone", (llsc_plain(0xAAAAAAAA, 0), sc_alone(0xBBBBBBBB)));
	reset(); llsc_twice(0, &rt1, &rt2); report("ll, sc, sc", rt1 * 10 + rt2);
	reset(); llsc_twice(1, &rt1, &rt2); report("ll, syscall, sc, sc", rt1 * 10 + rt2);
	reset(); report("ll, spin (interrupts), sc", llsc_spin(0xAAAAAAAA));
	reset(); report("ll, syscall, ll, sc", llsc_rearm(0xAAAAAAAA));
	reset(); report("syscall, sc", sc_after_syscall(0xAAAAAAAA));
	return 0;
}
