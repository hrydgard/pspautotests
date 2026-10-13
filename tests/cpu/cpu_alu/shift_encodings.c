#include <common.h>
#include <pspkernel.h>

// The shifts with their unused field set. srl with rs = 1 is rotr and srlv with sa = 1 is rotrv,
// but what do the other values of those fields do, and do sll/sra/sllv/srav ignore theirs?
// The instructions are generated at runtime, since the assembler won't encode them.

static unsigned int code[8] __attribute__((aligned(64)));

// rd = v0, rt = a0, rs = a1. v0 starts as deadbeef, so an instruction that does nothing shows.
static unsigned int run(unsigned int insn, unsigned int t, unsigned int s) {
	code[0] = 0x3c02dead;  // lui v0, 0xdead
	code[1] = 0x3442beef;  // ori v0, v0, 0xbeef
	code[2] = insn;
	code[3] = 0x03e00008;  // jr ra
	code[4] = 0x00000000;
	sceKernelDcacheWritebackRange(code, sizeof(code));
	sceKernelIcacheInvalidateRange(code, sizeof(code));
	return ((unsigned int (*)(unsigned int, unsigned int))code)(t, s);
}

static const struct {
	const char *name;
	unsigned int funct;
	int variable;
} shifts[] = {
	{ "sll", 0, 0 },
	{ "srl", 2, 0 },
	{ "sra", 3, 0 },
	{ "sllv", 4, 1 },
	{ "srlv", 6, 1 },
	{ "srav", 7, 1 },
};

int main(int argc, char *argv[]) {
	const unsigned int t = 0x87654321;
	for (int i = 0; i < ARRAY_SIZE(shifts); i++) {
		for (unsigned int f = 0; f < 32; f += 8) {
			printf("%-4s %2d-%2d:", shifts[i].name, f, f + 7);
			for (unsigned int j = f; j < f + 8; j++) {
				unsigned int insn;
				if (shifts[i].variable) {
					// The unused field is sa, the shift amount is in a1.
					insn = (5 << 21) | (4 << 16) | (2 << 11) | (j << 6) | shifts[i].funct;
				} else {
					// The unused field is rs, the shift amount is 4.
					insn = (j << 21) | (4 << 16) | (2 << 11) | (4 << 6) | shifts[i].funct;
				}
				printf(" %08x", run(insn, t, 4));
			}
			printf("\n");
		}
	}
	return 0;
}
