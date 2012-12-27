/*
FPU Test. Originally from jpcsp project:
http://code.google.com/p/jpcsp/source/browse/trunk/demos/src/fputest/main.c
Modified to perform automated tests.
*/

#include <common.h>
#include <math.h>

#include <pspkernel.h>

#define DEFINE_OP2(name) \
	float __attribute__((noinline)) op_##name(float x, float y) { \
		float result; \
		asm volatile(#name ".s %0, %1, %2" : "=f"(result) : "f"(x), "f"(y)); \
		return result; \
	}

#define DEFINE_OP1(name) \
	float __attribute__((noinline)) op_##name(float x) { \
		float result; \
		asm volatile(#name ".s %0, %1" : "=f"(result) : "f"(x)); \
		return result; \
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

float floatRelevantValues[] = {0, 1, -1, 1.5, -1.5, 1.6, -1.6, 1.4, -1.4, 2.0, -2.0, 4.0, INFINITY, -INFINITY, NAN, -NAN };
#define lengthof(v) (sizeof(v) / sizeof((v)[0]))

int isOperandOkay(const char *name, float a1) {
	// None of the ops actually support NaN, just crash.
	if (isnan(a1)) {
		return 0;
	}

	if (!strcmp(name, "sqrt")) {
		if (a1 <= 0) {
			return 0;
		}
	}

	return 1;
}

// We just get a crash for any of these values, so don't try it.
int areOperandsOkay(const char *name, float a1, float a2) {
	// TODO: Seems like NaN for either arg crashes all the ops.
	if (isnan(a1) || isnan(a2)) {
		return 0;
	}

	if (!strcmp(name, "add") || !strcmp(name, "sub")) {
		if (isinf(a1) && isinf(a2)) {
			return 0;
		}
	}
	if (!strcmp(name, "mul")) {
		if (a1 == 0.0 && isinf(a2)) {
			return 0;
		}
		if (a2 == 0.0 && isinf(a1)) {
			return 0;
		}
	}
	if (!strcmp(name, "div")) {
		if (a2 == 0.0) {
			return 0;
		}
		if (isinf(a1) && isinf(a2)) {
			return 0;
		}
	}

	return 1;
}

inline void runOperands(const char *name, float (*func)(float, float)) {
	int i, j;
	printf("%s.s:\n", name);
	for (i = 0; i < lengthof(floatRelevantValues); i++) {
		for (j = 0; j < lengthof(floatRelevantValues); j++) {
			printf("%f, %f => ", floatRelevantValues[i], floatRelevantValues[j]);

			if (areOperandsOkay(name, floatRelevantValues[i], floatRelevantValues[j])) {
				printf("%f\n", func(floatRelevantValues[i], floatRelevantValues[j]));
			} else {
				printf("SKIPPED\n");
			}
		}
	}
	printf("\n\n");
}

inline void runOperand(const char *name, float (*func)(float)) {
	int i;
	printf("%s.s:\n", name);
	for (i = 0; i < lengthof(floatRelevantValues); i++) {
		printf("%f => ", floatRelevantValues[i]);

		if (isOperandOkay(name, floatRelevantValues[i])) {
			printf("%f\n", func(floatRelevantValues[i]));
		} else {
			printf("SKIPPED\n");
		}
	}
	printf("\n\n");
}


#define CHECK_OP(op, expected) { float f = 0.0; f = op; printf("%s\n%f\n", #op " == " #expected, f);}

#define OUTPUT_2(OP) { runOperands(#OP, op_##OP); }
#define OUTPUT_1(OP) { runOperand(#OP, op_##OP); }

#define RINT_0  0
#define CAST_1  1
#define CEIL_2  2
#define FLOOR_3 3

DEFINE_OP2(add)
DEFINE_OP2(sub)
DEFINE_OP2(mul)
DEFINE_OP2(div)

DEFINE_OP1(sqrt)
DEFINE_OP1(abs)
DEFINE_OP1(neg)

int main(int argc, char *argv[]) {
	OUTPUT_2(add);
	OUTPUT_2(sub);
	OUTPUT_2(mul);
	OUTPUT_2(div);
	OUTPUT_1(sqrt);
	OUTPUT_1(abs);
	OUTPUT_1(neg);

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
