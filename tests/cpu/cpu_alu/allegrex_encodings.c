#include <common.h>
#include <pspkernel.h>

// The Allegrex bit ops (seb, seh, wsbh, wsbw, bitrev) are special3 with function 0x20 and the op
// in the sa field. What do the other sa values do, does function 0x18 decode the same way, and
// does the unused rs field matter? The instructions are generated at runtime, since the assembler
// won't encode them.

static unsigned int code[8] __attribute__((aligned(64)));

// rd = v0, rt = a0. v0 starts as deadbeef, so an instruction that does nothing shows.
static unsigned int run(unsigned int insn, unsigned int t) {
	code[0] = 0x3c02dead;  // lui v0, 0xdead
	code[1] = 0x3442beef;  // ori v0, v0, 0xbeef
	code[2] = insn;
	code[3] = 0x03e00008;  // jr ra
	code[4] = 0x00000000;
	sceKernelDcacheWritebackRange(code, sizeof(code));
	sceKernelIcacheInvalidateRange(code, sizeof(code));
	return ((unsigned int (*)(unsigned int))code)(t);
}

static unsigned int encode(unsigned int funct, unsigned int rs, unsigned int op) {
	return (0x1f << 26) | (rs << 21) | (4 << 16) | (2 << 11) | (op << 6) | funct;
}

int main(int argc, char *argv[]) {
	const unsigned int t = 0x8312a4f6;

	// The defined ops first, in case an undefined one traps.
	static const unsigned int known[] = { 0x02, 0x03, 0x10, 0x14, 0x18 };
	for (unsigned int rs = 0; rs < 32; rs += 31) {
		printf("funct 20, rs %2d:", rs);
		for (int i = 0; i < ARRAY_SIZE(known); i++) {
			printf(" %02x=%08x", known[i], run(encode(0x20, rs, known[i]), t));
		}
		printf("\n");
	}

	static const unsigned int functs[] = { 0x20, 0x18 };
	for (int f = 0; f < ARRAY_SIZE(functs); f++) {
		for (unsigned int rs = 0; rs < 32; rs += 31) {
			for (unsigned int op = 0; op < 32; op += 8) {
				printf("funct %02x, rs %2d, op %02x-%02x:", functs[f], rs, op, op + 7);
				fflush(stdout);
				for (unsigned int j = op; j < op + 8; j++) {
					printf(" %08x", run(encode(functs[f], rs, j), t));
					fflush(stdout);
				}
				printf("\n");
			}
		}
	}
	return 0;
}
