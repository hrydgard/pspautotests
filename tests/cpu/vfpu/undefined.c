#include <common.h>
#include <pspkernel.h>
#include "vfpu_common.h"

// VFPU encodings that nothing documents, where emulators have had to guess: single-lane dot
// products, vdet/vcrsp/vqmul at the other sizes, matrix registers whose row/column offset
// doesn't fit the size, vmmul.s, vtfm/vhtfm whose size doesn't match the transform, vcmov with
// imm3 = 7 or bit 20 set, and vmfvc from control register numbers past the defined ones.
//
// Each case runs with the whole register file loaded with S<m><c><r> = 1000 + 100m + 10c + r,
// and prints the registers that changed. The instructions are generated at runtime, since the
// assembler rejects most of them.

typedef struct {
	float regs[128];  // [m * 16 + c * 4 + r] is S<m><c><r>.
	unsigned int ccIn;
	unsigned int ccOut;
} __attribute__((aligned(16))) VfpuState;

extern void runVfpu(VfpuState *state, const unsigned int *code);

#define LV_ALL(base) \
	"lv.q C000, 0x000(" base ")\n" "lv.q C010, 0x010(" base ")\n" "lv.q C020, 0x020(" base ")\n" "lv.q C030, 0x030(" base ")\n" \
	"lv.q C100, 0x040(" base ")\n" "lv.q C110, 0x050(" base ")\n" "lv.q C120, 0x060(" base ")\n" "lv.q C130, 0x070(" base ")\n" \
	"lv.q C200, 0x080(" base ")\n" "lv.q C210, 0x090(" base ")\n" "lv.q C220, 0x0a0(" base ")\n" "lv.q C230, 0x0b0(" base ")\n" \
	"lv.q C300, 0x0c0(" base ")\n" "lv.q C310, 0x0d0(" base ")\n" "lv.q C320, 0x0e0(" base ")\n" "lv.q C330, 0x0f0(" base ")\n" \
	"lv.q C400, 0x100(" base ")\n" "lv.q C410, 0x110(" base ")\n" "lv.q C420, 0x120(" base ")\n" "lv.q C430, 0x130(" base ")\n" \
	"lv.q C500, 0x140(" base ")\n" "lv.q C510, 0x150(" base ")\n" "lv.q C520, 0x160(" base ")\n" "lv.q C530, 0x170(" base ")\n" \
	"lv.q C600, 0x180(" base ")\n" "lv.q C610, 0x190(" base ")\n" "lv.q C620, 0x1a0(" base ")\n" "lv.q C630, 0x1b0(" base ")\n" \
	"lv.q C700, 0x1c0(" base ")\n" "lv.q C710, 0x1d0(" base ")\n" "lv.q C720, 0x1e0(" base ")\n" "lv.q C730, 0x1f0(" base ")\n"

#define SV_ALL(base) \
	"sv.q C000, 0x000(" base ")\n" "sv.q C010, 0x010(" base ")\n" "sv.q C020, 0x020(" base ")\n" "sv.q C030, 0x030(" base ")\n" \
	"sv.q C100, 0x040(" base ")\n" "sv.q C110, 0x050(" base ")\n" "sv.q C120, 0x060(" base ")\n" "sv.q C130, 0x070(" base ")\n" \
	"sv.q C200, 0x080(" base ")\n" "sv.q C210, 0x090(" base ")\n" "sv.q C220, 0x0a0(" base ")\n" "sv.q C230, 0x0b0(" base ")\n" \
	"sv.q C300, 0x0c0(" base ")\n" "sv.q C310, 0x0d0(" base ")\n" "sv.q C320, 0x0e0(" base ")\n" "sv.q C330, 0x0f0(" base ")\n" \
	"sv.q C400, 0x100(" base ")\n" "sv.q C410, 0x110(" base ")\n" "sv.q C420, 0x120(" base ")\n" "sv.q C430, 0x130(" base ")\n" \
	"sv.q C500, 0x140(" base ")\n" "sv.q C510, 0x150(" base ")\n" "sv.q C520, 0x160(" base ")\n" "sv.q C530, 0x170(" base ")\n" \
	"sv.q C600, 0x180(" base ")\n" "sv.q C610, 0x190(" base ")\n" "sv.q C620, 0x1a0(" base ")\n" "sv.q C630, 0x1b0(" base ")\n" \
	"sv.q C700, 0x1c0(" base ")\n" "sv.q C710, 0x1d0(" base ")\n" "sv.q C720, 0x1e0(" base ")\n" "sv.q C730, 0x1f0(" base ")\n"

// Loads the registers and CC from a0, calls the code at a1, stores them back.
asm(
	".text\n"
	".set push\n"
	".set noreorder\n"
	".global runVfpu\n"
	".ent runVfpu\n"
	"runVfpu:\n"
	"addiu $sp, $sp, -16\n"
	"sw $ra, 0($sp)\n"
	"sw $s0, 4($sp)\n"
	"move $s0, $a0\n"
	LV_ALL("$s0")
	"lw $t0, 0x200($s0)\n"
	"mtvc $t0, $131\n"
	"jalr $a1\n"
	"nop\n"
	SV_ALL("$s0")
	"mfvc $t0, $131\n"
	"sw $t0, 0x204($s0)\n"
	"lw $s0, 4($sp)\n"
	"lw $ra, 0($sp)\n"
	"jr $ra\n"
	"addiu $sp, $sp, 16\n"
	".end runVfpu\n"
	".set pop\n"
);

static unsigned int code[16] __attribute__((aligned(64)));
static VfpuState state;

#define VPFXS_NONE 0xdc0000e4
#define VPFXT_NONE 0xdd0000e4
#define VPFXD_NONE 0xde000000
#define VRNDS_S000 0xd0200000
#define CC_DEFAULT 0x25

static void run(const char *name, unsigned int cc, const unsigned int *insns, int count) {
	for (int i = 0; i < 128; i++) {
		int m = i >> 4, c = (i >> 2) & 3, r = i & 3;
		state.regs[i] = (float)(1000 + 100 * m + 10 * c + r);
	}
	state.ccIn = cc;
	state.ccOut = 0xdeadbeef;

	int n = 0;
	code[n++] = VPFXS_NONE;
	code[n++] = VPFXT_NONE;
	code[n++] = VPFXD_NONE;
	for (int i = 0; i < count; i++) {
		code[n++] = insns[i];
	}
	code[n++] = 0x03e00008;  // jr ra
	code[n++] = 0x00000000;
	sceKernelDcacheWritebackRange(code, sizeof(code));
	sceKernelIcacheInvalidateRange(code, sizeof(code));

	printf("%s:", name);
	fflush(stdout);
	runVfpu(&state, code);

	for (int i = 0; i < 128; i++) {
		int m = i >> 4, c = (i >> 2) & 3, r = i & 3;
		float expected = (float)(1000 + 100 * m + 10 * c + r);
		if (memcmp(&state.regs[i], &expected, 4) != 0) {
			unsigned int bits;
			memcpy(&bits, &state.regs[i], 4);
			printf(" S%d%d%d=%08x(%g)", m, c, r, bits, state.regs[i]);
		}
	}
	if (state.ccOut != cc) {
		printf(" CC=%02x", state.ccOut);
	}
	printf("\n");
}

static void run1(const char *name, unsigned int insn) {
	run(name, CC_DEFAULT, &insn, 1);
}

typedef struct {
	const char *name;
	unsigned int insn;
} Case;

// vd = S000 / C000 / M000, vs = S100 / C100 / M100, vt = S200 / C200 / M200 unless noted.
static const Case dotCases[] = {
	{ "vdot.q S000, C100, C200 ", 0x64888480 },
	{ "vdot.p S000, C100, C200 ", 0x64880480 },
	{ "vdot.s S000, S100, S200 ", 0x64880400 },
	{ "vhdp.q S000, C100, C200 ", 0x66088480 },
	{ "vhdp.s S000, S100, S200 ", 0x66080400 },
	{ "vdet.p S000, C100, C200 ", 0x67080480 },
	{ "vdet.s                  ", 0x67080400 },
	{ "vdet.t                  ", 0x67088400 },
	{ "vdet.q                  ", 0x67088480 },
	{ "vcrsp.t C000, C100, C200", 0xf2888400 },
	{ "vqmul.q C000, C100, C200", 0xf2888480 },
	{ "vcrsp/vqmul.s           ", 0xf2880400 },
	{ "vcrsp/vqmul.p           ", 0xf2880480 },
};

// vmmul's vs field holds the transpose of the register written in the assembly.
static const Case matrixCases[] = {
	{ "vmmul.q M000, M100, M200", 0xf008a480 },
	{ "vmmul.t M000, M100, M200", 0xf008a400 },
	{ "vmmul.p M000, M100, M200", 0xf0082480 },
	{ "vmmul.s                 ", 0xf0082400 },
	// vmmov from register numbers with a row (bit 6) or column (bits 0-1) offset.
	{ "vmmov.q M000, 0x05      ", 0xf3800580 | 0x8000 },
	{ "vmmov.q M000, 0x06      ", 0xf3800680 | 0x8000 },
	{ "vmmov.q M000, 0x07      ", 0xf3800780 | 0x8000 },
	{ "vmmov.q M000, 0x44      ", 0xf3804480 | 0x8000 },
	{ "vmmov.q M000, 0x25      ", 0xf3802580 | 0x8000 },
	{ "vmmov.q M000, 0x64      ", 0xf3806480 | 0x8000 },
	{ "vmmov.t M000, 0x06      ", 0xf3808600 },
	{ "vmmov.t M000, 0x07      ", 0xf3808700 },
	{ "vmmov.t M000, 0x46      ", 0xf380c600 },
	{ "vmmov.t M000, 0x26      ", 0xf380a600 },
	{ "vmmov.p M000, 0x05      ", 0xf3800580 },
	{ "vmmov.p M000, 0x07      ", 0xf3800780 },
	{ "vmmov.p M000, 0x45      ", 0xf3804580 },
	{ "vmmov.p M000, 0x27      ", 0xf3802780 },
	// And to them.
	{ "vmidt.q 0x01            ", 0xf3838081 },
	{ "vmidt.q 0x42            ", 0xf38380c2 },
	{ "vmidt.t 0x02            ", 0xf3838002 },
	{ "vmidt.t 0x43            ", 0xf3838043 },
	{ "vmzero.q 0x03           ", 0xf3868083 },
	{ "vmone.t 0x22            ", 0xf3878022 },
	{ "vmscl.t 0x63, 0x26, S200", 0xf208a663 },
	{ "vmscl.q 0x01, M100, S200", 0xf2088481 },
	{ "vmmul.q 0x01, M100, M200", 0xf008a481 },
	{ "vmmul.t 0x02, M100, M200", 0xf008a402 },
	{ "vmmul.q M000, 0x25, M200", 0xf008a580 },
	{ "vmmul.q M000, M100, 0x09", 0xf009a480 },
	// vtfm and vhtfm where the vector size doesn't match the transform.
	{ "vtfm4.q C000, M100, C200", 0xf1888480 },
	{ "vtfm2 .t                ", 0xf0888400 },
	{ "vtfm2 .q                ", 0xf0888480 },
	{ "vtfm3 .s                ", 0xf1080400 },
	{ "vtfm3 .q                ", 0xf1088480 },
	{ "vtfm4 .s                ", 0xf1880400 },
	{ "vtfm4 .p                ", 0xf1880480 },
};

// imm3 in bits 16-18, tf in bit 19 (set: vcmovf), bit 20 is unused.
static const Case cmovCases[] = {
	{ "vcmovt.q C000, C100, 0  ", 0xd2a08480 },
	{ "vcmovt.q C000, C100, 6  ", 0xd2a68480 },
	{ "vcmovf.q C000, C100, 6  ", 0xd2ae8480 },
	{ "vcmovt.q imm3=7         ", 0xd2a78480 },
	{ "vcmovf.q imm3=7         ", 0xd2af8480 },
	{ "vcmovt.q 0, bit 20      ", 0xd2b08480 },
	{ "vcmovf.q 0, bit 20      ", 0xd2b88480 },
	{ "vcmovt.q 6, bit 20      ", 0xd2b68480 },
	{ "vcmovf.q 7, bit 20      ", 0xd2bf8480 },
	{ "vcmovt.s S000, S100, 2  ", 0xd2a20400 },
	{ "vcmovt.s S000, S100, 6  ", 0xd2a60400 },
};

static const unsigned int vmfvcRegs[] = { 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 23, 24, 31, 32, 35, 64, 127 };

int main(int argc, char *argv[]) {
	printf("-- dot products --\n");
	for (int i = 0; i < ARRAY_SIZE(dotCases); i++) {
		run1(dotCases[i].name, dotCases[i].insn);
	}

	printf("-- matrices --\n");
	for (int i = 0; i < ARRAY_SIZE(matrixCases); i++) {
		run1(matrixCases[i].name, matrixCases[i].insn);
	}

	printf("-- vcmov --\n");
	for (int i = 0; i < ARRAY_SIZE(cmovCases); i++) {
		run(cmovCases[i].name, 0x25, &cmovCases[i].insn, 1);
		run(cmovCases[i].name, 0x1a, &cmovCases[i].insn, 1);
	}

	// vrnds first, so the random generator's state (control registers 8-15) is known.
	printf("-- vmfvc --\n");
	for (int i = 0; i < ARRAY_SIZE(vmfvcRegs); i++) {
		char name[32];
		snprintf(name, sizeof(name), "vmfvc S000, %d", vmfvcRegs[i]);
		const unsigned int insns[2] = { VRNDS_S000 | (0x04 << 8), 0xd0508000 | (vmfvcRegs[i] << 8) };
		run(name, CC_DEFAULT, insns, 2);
	}
	return 0;
}
