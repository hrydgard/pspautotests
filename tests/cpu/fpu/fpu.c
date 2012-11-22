/*
FPU Test. Originally from jpcsp project:
http://code.google.com/p/jpcsp/source/browse/trunk/demos/src/fputest/main.c
Modified to perform automated tests.
*/

#include <common.h>

#include <pspkernel.h>

float __attribute__((noinline)) adds(float x, float y) {
	float result;
	asm volatile("add.s %0, %1, %2" : "=f"(result) : "f"(x), "f"(y));
	return result;
}

float __attribute__((noinline)) subs(float x, float y) {
	float result;
	asm volatile("sub.s %0, %1, %2" : "=f"(result) : "f"(x), "f"(y));
	return result;
}

float __attribute__((noinline)) muls(float x, float y) {
	float result;
	asm volatile("mul.s %0, %1, %2" : "=f"(result) : "f"(x), "f"(y));
	return result;
}

float __attribute__((noinline)) divs(float x, float y) {
	float result;
	asm volatile("div.s %0, %1, %2" : "=f"(result) : "f"(x), "f"(y));
	return result;
}

float __attribute__((noinline)) sqrts(float x) {
	float result;
	asm volatile("sqrt.s %0, %1" : "=f"(result) : "f"(x));
	return result;
}

float __attribute__((noinline)) abss(float x) {
	float result;
	asm volatile("abs.s %0, %1" : "=f"(result) : "f"(x));
	return result;
}

float __attribute__((noinline)) negs(float x) {
	float result;
	asm volatile("neg.s %0, %1" : "=f"(result) : "f"(x));
	return result;
}

int __attribute__((noinline)) cvtws(float x, int rm) {
	float resultFloat;
	asm volatile("ctc1 %0, $31" : : "r"(rm));
	asm volatile("cvt.w.s %0, %1" : "=f"(resultFloat) : "f"(x));
	int result = *((int *) &resultFloat);
	return result;
}

/* these must be VFPU
const char *cmpNames[16] = {
  "FL",
  "EQ",
  "LT",
  "LE",
  "TR",
  "NE",
  "GE",
  "GT",
  "EZ",
  "EN",
  "EI", 
  "ES",
  "NZ",
  "NN",
  "NI",
  "NS",
};
*/

const char *cmpNames[16] = {
  "f",
  "un",
  "eq",
  "ueq",
  "olt",
  "ult",
  "ole",
  "ule",
  "sf",
  "ngle",
  "seq", 
  "ngl",
  "lt",
  "nge",
  "le",
  "ngt",
};

void testCompare(float a, float b) {
  printf("=== Comparing %f, %f ===\n", a, b);
  int temp, i;
  unsigned int res[16];
  memset(res, 0, sizeof(res));
  asm volatile(
    "c.f.s %1, %2\n" "cfc1 %3, $31\n" "sw %3, 0+%0\n"
    "c.un.s %1, %2\n" "cfc1 %3, $31\n" "sw %3, 4+%0\n"
    "c.eq.s %1, %2\n" "cfc1 %3, $31\n" "sw %3, 8+%0\n"
    "c.ueq.s %1, %2\n" "cfc1 %3, $31\n" "sw %3, 12+%0\n"

    "c.olt.s %1, %2\n" "cfc1 %3, $31\n" "sw %3, 16+%0\n"
    "c.ult.s %1, %2\n" "cfc1 %3, $31\n" "sw %3, 20+%0\n"
    "c.ole.s %1, %2\n" "cfc1 %3, $31\n" "sw %3, 24+%0\n"
    "c.ule.s %1, %2\n" "cfc1 %3, $31\n" "sw %3, 28+%0\n"

    "c.sf.s %1, %2\n" "cfc1 %3, $31\n" "sw %3, 32+%0\n"
    "c.ngle.s %1, %2\n" "cfc1 %3, $31\n" "sw %3, 36+%0\n"
    "c.seq.s %1, %2\n" "cfc1 %3, $31\n" "sw %3, 40+%0\n"
    "c.ngl.s %1, %2\n" "cfc1 %3, $31\n" "sw %3, 44+%0\n"

    "c.lt.s %1, %2\n" "cfc1 %3, $31\n" "sw %3, 48+%0\n"
    "c.nge.s %1, %2\n" "cfc1 %3, $31\n" "sw %3, 52+%0\n"
    "c.le.s %1, %2\n" "cfc1 %3, $31\n" "sw %3, 56+%0\n"
    "c.ngt.s %1, %2\n" "cfc1 %3, $31\n" "sw %3, 60+%0\n"

    : "=m"(res[0]) : "f"(a), "f"(b), "r"(temp)
  );
  /*
  asm volatile(
    "c.eq.s %1, %2\n\t"
    "bc0f
    "sw %3, 4+%0\n\t"

    "c.lt.s %1, %2\n\t"
    "mfc0 %3, $31\n\t"
    "sw %3, 8+%0\n\t"

    "c.le.s %1, %2\n\t"
    "mfc0 %3, $31\n\t"
    "sw %3, 12+%0\n\t"

    : "=m"(res[0]) : "f"(a), "f"(b), "r"(temp)
  );*/
  for (i = 0; i < 16; i++) {
    if (1) {
      //simple mode, only condition flag
      printf("%f %f %s: %s\n", a, b, cmpNames[i], ((res[i]>>23)&1) ? "T" : "F");
    } else {
      printf("%f %f %s: %08x\n", a, b, cmpNames[i], res[i]);
    }
  }
}



#define CHECK_OP(op, expected) { float f = 0.0; f = op; printf("%s\n%f\n", #op " == " #expected, f);}

#define RINT_0  0
#define CAST_1  1
#define CEIL_2  2
#define FLOOR_3 3

int main(int argc, char *argv[]) {
	CHECK_OP(adds(1.0, 1.0), 2.0);
	CHECK_OP(subs(3.0, 1.0), 2.0);
	CHECK_OP(muls(2.0, 1.0), 2.0);
	CHECK_OP(divs(4.0, 2.0), 2.0);
	CHECK_OP(abss(+2.0), 2.0);
	CHECK_OP(abss(-2.0), 2.0);
	CHECK_OP(negs(negs(+2.0)), 2.0);
	CHECK_OP(sqrts(4.0), 2.0);

	CHECK_OP(cvtws(1.1, RINT_0), 1);
	CHECK_OP(cvtws(1.1, CAST_1), 1);
	CHECK_OP(cvtws(1.1, CEIL_2), 2);
	CHECK_OP(cvtws(1.1, FLOOR_3), 1);

	CHECK_OP(cvtws(-1.1, RINT_0), -1);
	CHECK_OP(cvtws(-1.1, CAST_1), -1);
	CHECK_OP(cvtws(-1.1, CEIL_2), -1);
	CHECK_OP(cvtws(-1.1, FLOOR_3), -2);

	CHECK_OP(cvtws(1.9, RINT_0), 2);
	CHECK_OP(cvtws(1.9, CAST_1), 1);
	CHECK_OP(cvtws(1.9, CEIL_2), 2);
	CHECK_OP(cvtws(1.9, FLOOR_3), 1);

	CHECK_OP(cvtws(1.5, RINT_0), 2);
	CHECK_OP(cvtws(2.5, RINT_0), 2);
	CHECK_OP(cvtws(3.5, RINT_0), 2);
	CHECK_OP(cvtws(4.5, RINT_0), 2);
	CHECK_OP(cvtws(1.5, CAST_1), 1);
	CHECK_OP(cvtws(2.5, CAST_1), 1);
	CHECK_OP(cvtws(1.5, CEIL_2), 2);
	CHECK_OP(cvtws(1.5, FLOOR_3), 1);

  testCompare(0.0f, 0.0f);
  testCompare(1.0f, 1.0f);
  testCompare(1.0f, 2.0f);
  testCompare(1.0f, -2.0f);

	return 0;
}
