#include <pspkernel.h>

PSP_MODULE_INFO("cpu test", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define OP1(TYPE) int __attribute__((noinline)) op_##TYPE(int x       ) { int result; asm volatile(#TYPE " %0, %1" : "=r"(result) : "r"(x)); return result; }
#define OP2(TYPE) int __attribute__((noinline)) op_##TYPE(int x, int y) { int result; asm volatile(#TYPE " %0, %1, %2" : "=r"(result) : "r"(x), "r"(y)); return result; }

#define OPX_TEST_START() Kprintf("--\n");
#define OP1_TEST_X(TYPE, a) Kprintf("%s 0x%08X -> %d\n", #TYPE, a, op_##TYPE(a));
#define OP1_TEST(TYPE, a) Kprintf("%s %d -> %d\n", #TYPE, a, op_##TYPE(a));
#define OP2_TEST(TYPE, a, b) Kprintf("%s %d, %d -> %d\n", #TYPE, a, b, op_##TYPE(a, b));

#define OP2_TEST_SET(TYPE) \
	OPX_TEST_START(); \
	OP2_TEST(TYPE, 1, 7); \
	OP2_TEST(TYPE, 3, 1); \
	OP2_TEST(TYPE, -1, 0); \
	OP2_TEST(TYPE, -1, -10); \
	OP2_TEST(TYPE, 0, 0); \
	

OP2(max)
OP2(min)

OP2(add)
OP2(addu)
OP2(sub)
OP2(subu)

OP1(clo)
OP1(clz)

int main(int argc, char *argv[]) {
	OP2_TEST_SET(max);
	OP2_TEST_SET(min);
	OP2_TEST_SET(add);
	OP2_TEST_SET(addu);
	OP2_TEST_SET(sub);
	OP2_TEST_SET(subu);
	
	OPX_TEST_START();
	OP1_TEST_X(clo, 0x00000000)
	OP1_TEST_X(clo, 0x80000000)
	OP1_TEST_X(clo, 0xF0000000)
	OP1_TEST_X(clo, 0xF000000F)
	OP1_TEST_X(clo, 0xFFFF000F)
	OP1_TEST_X(clo, 0xF7FF000F)
	OP1_TEST_X(clo, 0xFFFFFFFF)

	OPX_TEST_START();
	OP1_TEST_X(clz, 0x00000000)
	OP1_TEST_X(clz, 0x0000000F)
	OP1_TEST_X(clz, 0xF000000F)
	OP1_TEST_X(clz, 0x0000007F)
	OP1_TEST_X(clz, 0xFFFFFFFF)

	return 0;
}