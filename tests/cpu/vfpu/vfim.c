#include <common.h>
#include "vfpu_common.h"

// vfim.s turns a 16-bit immediate, a half float, into a float. Written as raw words, as the
// assembler only takes float literals: the denormal, inf and NaN halves are the interesting ones.

#define VFIM_TEST(name, imm) \
	static unsigned int __attribute__((noinline)) name(void) { \
		unsigned int result; \
		asm volatile ( \
			".word 0xdf800000 | " #imm "\n" \
			"mfv    %0, S000\n" \
			: "=r" (result) : : "memory" \
		); \
		return result; \
	}

VFIM_TEST(t_0000, 0x0000)
VFIM_TEST(t_8000, 0x8000)
VFIM_TEST(t_0001, 0x0001)
VFIM_TEST(t_0090, 0x0090)
VFIM_TEST(t_03ff, 0x03ff)
VFIM_TEST(t_8090, 0x8090)
VFIM_TEST(t_0400, 0x0400)
VFIM_TEST(t_3c00, 0x3c00)
VFIM_TEST(t_7bff, 0x7bff)
VFIM_TEST(t_7c00, 0x7c00)
VFIM_TEST(t_fc00, 0xfc00)
VFIM_TEST(t_7c01, 0x7c01)
VFIM_TEST(t_7e00, 0x7e00)
VFIM_TEST(t_fdff, 0xfdff)

static const struct {
	unsigned int imm;
	unsigned int (*func)(void);
} tests[] = {
	{ 0x0000, t_0000 }, { 0x8000, t_8000 }, { 0x0001, t_0001 }, { 0x0090, t_0090 },
	{ 0x03ff, t_03ff }, { 0x8090, t_8090 }, { 0x0400, t_0400 }, { 0x3c00, t_3c00 },
	{ 0x7bff, t_7bff }, { 0x7c00, t_7c00 }, { 0xfc00, t_fc00 }, { 0x7c01, t_7c01 },
	{ 0x7e00, t_7e00 }, { 0xfdff, t_fdff },
};

int main(int argc, char **argv) {
	for (size_t i = 0; i < ARRAY_SIZE(tests); i++)
		printf("vfim.s %04x: %08x\n", tests[i].imm, tests[i].func());
	return 0;
}
