#include <common.h>

#include <pspkernel.h>

#define lengthof(vector) (sizeof((vector)) / sizeof((vector)[0]))

#define OP_r_r(TYPE)  int __attribute__((noinline)) op_##TYPE(int x       ) { int result; asm volatile(#TYPE " %0, %1" : "=r"(result) : "r"(x)); return result; }
#define OP_r_rr(TYPE) int __attribute__((noinline)) op_##TYPE(int x, int y) { int result; asm volatile(#TYPE " %0, %1, %2" : "=r"(result) : "r"(x), "r"(y)); return result; }

#define OP_r_ri_x(TYPE, KEY, VALUE) int __attribute__((noinline)) op_##TYPE##KEY(int x) { int result; asm volatile(#TYPE " %0, %1, " #VALUE : "=r"(result) : "r"(x)); return result; }
#define OP_r_ri(TYPE) OP_r_ri_x(TYPE, 0, 0) OP_r_ri_x(TYPE, 1, 1) OP_r_ri_x(TYPE, 2, 2) OP_r_ri_x(TYPE, 3, 3) OP_r_ri_x(TYPE, 4, -1) OP_r_ri_x(TYPE, 5, -2) OP_r_ri_x(TYPE, 6, -32768) OP_r_ri_x(TYPE, 7, 32767) OP_r_ri_x(TYPE, 8, 11123)
#define OP_r_ri_u(TYPE) OP_r_ri_x(TYPE, 0, 0) OP_r_ri_x(TYPE, 1, 1) OP_r_ri_x(TYPE, 2, 2) OP_r_ri_x(TYPE, 3, 3) OP_r_ri_x(TYPE, 4, 0x1234) OP_r_ri_x(TYPE, 5, 0x2345) OP_r_ri_x(TYPE, 6, 0x3456) OP_r_ri_x(TYPE, 7, 0xffff) OP_r_ri_x(TYPE, 8, 0x7fff)
#define OP_r_rp(TYPE) OP_r_ri_x(TYPE, 0, 0) OP_r_ri_x(TYPE, 1, 1) OP_r_ri_x(TYPE, 2, 2) OP_r_ri_x(TYPE, 3, 3) OP_r_ri_x(TYPE, 4, 7) OP_r_ri_x(TYPE, 5, 15) OP_r_ri_x(TYPE, 6, 29) OP_r_ri_x(TYPE, 7, 30) OP_r_ri_x(TYPE, 8, 31)

#define TEST_START(TYPE) printf("%s: ", #TYPE);
#define TEST_END(TYPE) printf("\n");

#define TEST_r_r(TYPE, x    ) printf("0x%08X, ", op_##TYPE(x));
#define TEST_r_rr(TYPE, x, y) printf("0x%08X, ", op_##TYPE(x, y));
#define TEST_r_ri_x(TYPE, KEY, x   ) printf("0x%08X, ", op_##TYPE##KEY(x));
#define TEST_r_ri(TYPE, x   ) { TEST_r_ri_x(TYPE, 0, x) TEST_r_ri_x(TYPE, 1, x) TEST_r_ri_x(TYPE, 2, x) TEST_r_ri_x(TYPE, 3, x) TEST_r_ri_x(TYPE, 4, x) TEST_r_ri_x(TYPE, 5, x) TEST_r_ri_x(TYPE, 6, x) TEST_r_ri_x(TYPE, 7, x) TEST_r_ri_x(TYPE, 8, x) }

const int arithmeticValues[] = { 0, 1, 2, 0x81, 0x7f, 0x8123, 0x7fff, 2147483647, -1, -2, -2147483648 };
#define TEST_r_rr_SET(TYPE) { TEST_START(TYPE); int x, y; for (x = 0; x < lengthof(arithmeticValues); x++) for (y = 0; y < lengthof(arithmeticValues); y++) TEST_r_rr(TYPE, arithmeticValues[x], arithmeticValues[y]); TEST_END(TYPE); }
#define TEST_r_r_SET(TYPE) { TEST_START(TYPE); int x; for (x = 0; x < lengthof(arithmeticValues); x++) TEST_r_r(TYPE, arithmeticValues[x]); TEST_END(TYPE); }
#define TEST_r_ri_SET(TYPE) { TEST_START(TYPE); int x; for (x = 0; x < lengthof(arithmeticValues); x++) TEST_r_ri(TYPE, arithmeticValues[x]); TEST_END(TYPE); }
#define TEST_r_rp_SET TEST_r_ri_SET

__attribute__ ((noinline)) unsigned int fixed_ror(unsigned int value) {
	int ret;
	asm volatile (
		"ror %0, %1, 4\n"
		: "=r"(ret) : "r"(value)
	);
	return ret;
}

__attribute__ ((noinline)) unsigned int fixed_rorv(unsigned int value, int offset) {
	int ret;
	asm volatile (
		"rorv %0, %1, %2\n"
		: "=r"(ret) : "r"(value), "r"(offset)
	);
	return ret;
}

void test_mul64() {
	volatile unsigned long long a = 0x8234567812345678ULL;
	volatile unsigned long long b = 0x2345678123456783ULL;
	volatile signed long long c = 0x8234567812345678ULL;
	volatile signed long long d = 0xF234567812345678ULL;
	printf("%llu\n", a * b);
	printf("%llu\n", b * c - 1);
	printf("%llu\n", (c * d) >> 7);
}

void test_div() {
	volatile int a = 1 << 31; 
	volatile int b = -1;
	volatile int c = 100;
	volatile int d = 3;
	printf("%08x\n", a / b);
	printf("%08x\n", c / d); 
}

// Arithmetic operations.
OP_r_rr(add)
OP_r_rr(addu)
OP_r_rr(sub)
OP_r_rr(subu)
OP_r_ri(addi)
OP_r_ri(addiu)

// Logical Operations.
OP_r_rr(and);
OP_r_rr(or);
OP_r_rr(xor);
OP_r_rr(nor);
OP_r_ri_u(andi);
OP_r_ri_u(ori);
OP_r_ri_u(xori);

// Shift Left/Right Logical/Arithmethic (Variable).
OP_r_rp(sll);
OP_r_rp(sra);
OP_r_rp(srl);
OP_r_rp(ror);
OP_r_rr(sllv);
OP_r_rr(srav);
OP_r_rr(srlv);
OP_r_rr(rotrv);

// Set Less Than (Immediate) (Unsigned).
OP_r_rr(slt);
OP_r_rr(sltu);
OP_r_ri(slti);
OP_r_ri_u(sltu);

// Load Upper Immediate.
OP_r_r(lui)

// Sign Extend Byte/Half word.
OP_r_r(seb)
OP_r_r(seh)

// BIT REVerse.
OP_r_r(bitrev)

// MAXimum/MINimum.
OP_r_rr(max)
OP_r_rr(min)

// Count Leading Ones/Zeros in word.
OP_r_r(clo)
OP_r_r(clz)

// Word Swap Bytes Within Halfwords/Words.
OP_r_r(wsbh)
OP_r_r(wsbw)


int main(int argc, char *argv[]) {
	// Arithmetic operations.
	TEST_r_rr_SET(add);
	TEST_r_rr_SET(addu);
	TEST_r_rr_SET(sub);
	TEST_r_rr_SET(subu);
	TEST_r_ri_SET(addi);
	TEST_r_ri_SET(addiu);
	
	// Logical Operations.
	TEST_r_rr_SET(and);
	TEST_r_rr_SET(or);
	TEST_r_rr_SET(xor);
	TEST_r_rr_SET(nor);
	TEST_r_ri_SET(andi);
	TEST_r_ri_SET(ori);
	TEST_r_ri_SET(xori);
	
	// Shift Left/Right Logical/Arithmethic (Variable).
	TEST_r_rp_SET(sll);
	TEST_r_rp_SET(sra);
	TEST_r_rp_SET(srl);
	TEST_r_rp_SET(ror);
	TEST_r_rr_SET(sllv);
	TEST_r_rr_SET(srav);
	TEST_r_rr_SET(srlv);
	TEST_r_rr_SET(rotrv);

	// Set Less Than (Immediate) (Unsigned).
	TEST_r_rr_SET(slt);
	TEST_r_rr_SET(sltu);
	TEST_r_ri_SET(slti);
	TEST_r_ri_SET(sltu);

	// Load Upper Immediate.
	TEST_r_r_SET(lui)
	
	// Sign Extend Byte/Half word.
	TEST_r_r_SET(seb)
	TEST_r_r_SET(seh)

	// BIT REVerse.
	TEST_r_r_SET(bitrev);

	// MAXimum/MINimum.
	TEST_r_rr_SET(max);
	TEST_r_rr_SET(min);

	// Count Leading Ones/Zeros in word.
	TEST_r_r_SET(clo);
	TEST_r_r_SET(clz);

	// Word Swap Bytes Within Halfwords/Words.
	TEST_r_r_SET(wsbh)
	TEST_r_r_SET(wsbw)

	// Must test individually:
	// ext, ins
	// movz, movn
	// mfhi, mflo, mthi, mtlo
	// div, divu
	// mult, multu, madd, maddu, msub, msubu

	// Other
	test_mul64();
	test_div();

	return 0;
}
