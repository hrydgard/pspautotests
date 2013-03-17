/*
FPU Test. Originally from jpcsp project:
http://code.google.com/p/jpcsp/source/browse/trunk/demos/src/fputest/main.c
Modified to perform automated tests.
*/


// For some reason, vectors on the stack all are unaligned. So I've pulled them out
// into globals, which is horribly ugly. But it'll do for now.


#include <common.h>

#include <pspkernel.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>

#include "vfpu_common.h"

ALIGN16 ScePspFVector4 v0, v1, v2;
ALIGN16 ScePspFVector4 matrix[4];

void initValues() {
	// Reset output values
	v0.x = 1001;
	v0.y = 1002;
	v0.z = 1003;
	v0.w = 1004;

	v1.x = 17;
	v1.y = 13;
	v1.z = -5;
	v1.w = 11;

	v2.x = 3;
	v2.y = -7;
	v2.z = -15;
	v2.w = 19;
}

const ALIGN16 ScePspFVector4 testVectors[8] = {
	{1.0f, 2.0f, 3.0f, 4.0f},
	{5.0f, 6.0f, -7.0f, -8.0f},
	{-1.0f, -2.0f, -3.0f, -4.0f},
	{-4.0f, -5.0f, 6.0f, 7.0f},
	{-0.001f, 1000.0f, 0.0f, INFINITY},
	{0.02f, 10000000.0f, -0.0f, NAN},
	{-NAN, INFINITY, 90.0f, -NAN},
	{NAN, -NAN, 90.0f, INFINITY},
};

void NOINLINE vcopy(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C730, %1\n"
		"vmov.q C720, C730\n"
		"sv.q   C720, %0\n"
		: "+m" (*v0) : "m" (*v1)
	);
}

#define VDOT(vdotX, VDOTX) \
void NOINLINE vdotX(ScePspFVector4 *v0, ScePspFVector4 *v1, ScePspFVector4 *v2) { \
	asm volatile ( \
		"lv.q   C100, %1\n" \
		"lv.q   C200, %2\n" \
		VDOTX " S000, C100, C200\n" \
		"sv.q   C000, %0\n" \
		: "+m" (*v0) : "m" (*v1), "m" (*v2) \
	); \
}

VDOT(vdotp, "vdot.p");
VDOT(vdott, "vdot.t");
VDOT(vdotq, "vdot.q");

void NOINLINE vzero_q(ScePspFVector4* v0) {
	__asm__ volatile (
		"vzero.q R000\n"
		"sv.q   R000, 0x00+%0\n"
		: "+m" (*v0)
		);
}

void NOINLINE vone_q(ScePspFVector4* v0) {
	__asm__ volatile (
		"vone.q R000\n"
		"sv.q   R000, 0x00+%0\n"
		: "+m" (*v0)
		);
}

void NOINLINE vsclq(ScePspFVector4 *v0, ScePspFVector4 *v1, ScePspFVector4 *v2) {
	asm volatile (
		"lv.q   C100, %1\n"
		"lv.q   C200, %2\n"
		"vscl.q C300, C100, S200\n"
		"sv.q   C300, %0\n"

		: "+m" (*v0) : "m" (*v1), "m" (*v2)
	);
}

void NOINLINE vfim(ScePspFVector4 *v0) {
	asm volatile (
		"vfim.s	 s500, 0.011111111111111112\n"
		"vfim.s	 s501, -0.011111111111111112\n"
		"vfim.s	 s502, inf\n"
		"vfim.s	 s503, nan\n"
		"sv.q    C500, %0\n"

		: "+m" (*v0)
	);
}

void checkConstants() {
	static ALIGN16 ScePspFVector4 v[5];
	int n;
	asm volatile(
		"vcst.s S000, VFPU_HUGE\n"
		"vcst.s S010, VFPU_SQRT2\n"
		"vcst.s S020, VFPU_SQRT1_2\n"
		"vcst.s S030, VFPU_2_SQRTPI\n"
		"vcst.s S001, VFPU_2_PI\n"
		"vcst.s S011, VFPU_1_PI\n"
		"vcst.s S021, VFPU_PI_4\n"
		"vcst.s S031, VFPU_PI_2\n"
		"vcst.s S002, VFPU_PI\n"
		"vcst.s S012, VFPU_E\n"
		"vcst.s S022, VFPU_LOG2E\n"
		"vcst.s S032, VFPU_LOG10E\n"
		"vcst.s S003, VFPU_LN2\n"
		"vcst.s S013, VFPU_LN10\n"
		"vcst.s S023, VFPU_2PI\n"
		"vcst.s S033, VFPU_PI_6\n"
		"vcst.s S100, VFPU_LOG10TWO\n"
		"vcst.s S110, VFPU_LOG2TEN\n"
		"vcst.s S120, VFPU_SQRT3_2\n"
		"viim.s S130, 0\n"
		"sv.q   R000, 0x00+%0\n"
		"sv.q   R001, 0x10+%0\n"
		"sv.q   R002, 0x20+%0\n"
		"sv.q   R003, 0x30+%0\n"
		"sv.q   R100, 0x40+%0\n"
		: "+m" (v[0])
	);
	for (n = 0; n < 5; n++) {
		printVector("vcst.s", &v[n]);
	}
}

void NOINLINE vsgn(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vsgn.q C200, C100\n"
		"sv.q   C200, %0\n"
		: "+m" (*v0) : "m" (*v1)
		);
}

void NOINLINE vrcp(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vrcp.q C200, C100\n"
		"sv.q   C200, %0\n"
		: "+m" (*v0) : "m" (*v1)
		);
}

void NOINLINE vbfy1_p(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vbfy1.p C200, C100\n"
		"sv.q   C200, %0\n"
		: "+m" (*v0) : "m" (*v1)
		);
}

void NOINLINE vbfy1_q(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vbfy1.q C200, C100\n"
		"sv.q   C200, %0\n"
		: "+m" (*v0) : "m" (*v1)
		);
}

void NOINLINE vbfy2_q(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vbfy2.q C200, C100\n"
		"sv.q   C200, %0\n"

		: "+m" (*v0) : "m" (*v1)
		);
}

void NOINLINE vsat0(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vsat0.q C200, C100\n"
		"sv.q   C200, %0\n"

		: "+m" (*v0) : "m" (*v1)
		);
}

void NOINLINE vmax(ScePspFVector4 *v0, ScePspFVector4 *v1, ScePspFVector4 *v2) {
	asm volatile (
		"lv.q   C100, %1\n"
		"lv.q   C200, %2\n"
		"vmax.q C300, C200, C100\n"
		"sv.q   C300, %0\n"

		: "+m" (*v0): "m" (*v1), "m" (*v2)
		);
}

void NOINLINE vsge(ScePspFVector4 *v0, ScePspFVector4 *v1, ScePspFVector4 *v2) {
	asm volatile (
		"lv.q   C100, %1\n"
		"lv.q   C200, %2\n"
		"vsge.q C300, C200, C100\n"
		"sv.q   C300, %0\n"

		: "+m" (*v0): "m" (*v1), "m" (*v2)
		);
}

void NOINLINE vslt(ScePspFVector4 *v0, ScePspFVector4 *v1, ScePspFVector4 *v2) {
	asm volatile (
		"lv.q   C100, %1\n"
		"lv.q   C200, %2\n"
		"vslt.q C300, C200, C100\n"
		"sv.q   C300, %0\n"

		: "+m" (*v0): "m" (*v1), "m" (*v2)
		);
}

void NOINLINE vmin(ScePspFVector4 *v0, ScePspFVector4 *v1, ScePspFVector4 *v2) {
	asm volatile (
		"lv.q   C100, %1\n"
		"lv.q   C200, %2\n"
		"vmin.q C300, C100, C200\n"
		"sv.q   C300, %0\n"

		: "+m" (*v0): "m" (*v1), "m" (*v2)
		);
}

void NOINLINE vcsrp_t(ScePspFVector4 *vleft, ScePspFVector4 *vright, ScePspFVector4 *vresult) {
	asm volatile (
		"lv.q R500, 0x00+%1\n"
		"lv.q R600, 0x00+%2\n"

		"vcrsp.t R100, R500, R600\n"

		"sv.q    R100, 0x00+%0\n"
		: "+m" (*vresult) : "m" (*vleft), "m" (*vright)
		);
}

void NOINLINE vqmul_q(ScePspFVector4 *vleft, ScePspFVector4 *vright, ScePspFVector4 *vresult) {
	asm volatile (
		"lv.q R500, 0x00+%1\n"
		"lv.q R600, 0x00+%2\n"

		"vqmul.q R100, R500, R600\n"

		"sv.q    R100, 0x00+%0\n"
		: "+m" (*vresult) : "m" (*vleft), "m" (*vright)
		);
}


void checkVadd() {
	printf("TODO!\n");
}

void checkVsub() {
	printf("TODO!\n");
}

void checkVdiv() {
	printf("TODO!\n");
}

void checkVmul() {
	printf("TODO!\n");
}

void checkVmmov() {
	printf("TODO!\n");
}

void checkVsgn() {
	static ALIGN16 ScePspFVector4 vIn =
	{-0.0f, 1.3f, -INFINITY, NAN};
	static ALIGN16 ScePspFVector4 vIn2 =
	{0.0f, -1.0f, INFINITY, -NAN};
	static ALIGN16 ScePspFVector4 vOut =
	{0.0f, 0.0f, 0.0f, 0.0f};

	vsgn(&vOut, &vIn);
	printf("vsgn.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vsgn(&vOut, &vIn2);
	printf("vsgn.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
}

void checkVrcp() {
	static ALIGN16 ScePspFVector4 vIn =
	{0.0f, -1.3f, INFINITY, NAN};
	static ALIGN16 ScePspFVector4 vOut =
	{0.0f, 0.0f, 0.0f, 0.0f};

	vrcp(&vOut, &vIn);
	printVector("vrcp.q", &vOut);
}

void checkVbfy1() {
	static ALIGN16 ScePspFVector4 vIn =
	{10.0f, -5.f, 7.f, 3.f};
	static ALIGN16 ScePspFVector4 vIn2 =
	{6.0f, -46.f, 7.f, -INFINITY};
	static ALIGN16 ScePspFVector4 vOut =
	{0.0f, 0.0f, 0.0f, 0.0f};

	vbfy1_p(&vOut, &vIn);
	printVector("vbfy1.p", &vOut);
	vbfy1_p(&vOut, &vIn2);
	printVector("vbfy1.p", &vOut);
	vbfy1_q(&vOut, &vIn);
	printVector("vbfy1.q", &vOut);
	vbfy1_q(&vOut, &vIn2);
	printVector("vbfy1.q", &vOut);
}

void checkVbfy2() {
	static ALIGN16 ScePspFVector4 vIn =
	{20.0f, -1.f, 4.f, 9.f};
	static ALIGN16 ScePspFVector4 vIn2 =
	{11.0f, -3.f, 8.f, NAN};
	static ALIGN16 ScePspFVector4 vOut =
	{0.0f, 0.0f, 0.0f, 0.0f};

	vbfy2_q(&vOut, &vIn);
	printf("vbfy2.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vbfy2_q(&vOut, &vIn2);
	printf("vbfy2.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
}

void checkVsat0() {
	static ALIGN16 ScePspFVector4 vIn =
	{0.0f, -1.3f, INFINITY, NAN};
	static ALIGN16 ScePspFVector4 vIn2 =
	{10e10f, 1.000001f, -INFINITY, -NAN};
	static ALIGN16 ScePspFVector4 vOut =
	{0.0f, 0.0f, 0.0f, 0.0f};

	vsat0(&vOut, &vIn);
	printf("vsat0.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vsat0(&vOut, &vIn2);
	printf("vsat0.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
}

void checkVmin() {
	static ALIGN16 ScePspFVector4 vIn =
	{3.0f, -4.0f, NAN, 86.0f};
	static ALIGN16 ScePspFVector4 vIn2 =
	{4.0f, -5.0f, 0.0f, NAN};
	static ALIGN16 ScePspFVector4 vOut =
	{0.0f, 0.0f, 0.0f, 0.0f};

	vmin(&vOut, &vIn, &vIn2);
	printf("vmin.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
}

void checkVmax() {
	static ALIGN16 ScePspFVector4 vIn =
	{3.0f, -4.0f, NAN, 86.0f};
	static ALIGN16 ScePspFVector4 vIn2 =
	{4.0f, -5.0f, 0.0f, NAN};
	static ALIGN16 ScePspFVector4 vOut =
	{0.0f, 0.0f, 0.0f, 0.0f};

	vmax(&vOut, &vIn, &vIn2);
	printf("vmax.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
}

void checkVsge() {
	static ALIGN16 ScePspFVector4 vIn =
	{3.0f, -4.0f, NAN, 86.0f};
	static ALIGN16 ScePspFVector4 vIn2 =
	{4.0f, -5.0f, 0.0f, NAN};
	static ALIGN16 ScePspFVector4 vOut =
	{0.0f, 0.0f, 0.0f, 0.0f};

	vsge(&vOut, &vIn, &vIn2);
	printVector("vsge.q", &vOut);
}



void checkVslt() {
	static ALIGN16 ScePspFVector4 vIn =
	{3.0f, -4.0f, NAN, 86.0f};
	static ALIGN16 ScePspFVector4 vIn2 =
	{4.0f, -5.0f, 0.0f, NAN};
	static ALIGN16 ScePspFVector4 vOut =
	{0.0f, 0.0f, 0.0f, 0.0f};

	vslt(&vOut, &vIn, &vIn2);
	printVector("vslt.q", &vOut);
}

void NOINLINE checkViim() {
	int n;
	static ALIGN16 ScePspFVector4 v[4];

	asm volatile(
		"viim.s S000, 0\n"
		"viim.s S010, 1\n"
		"viim.s S020, -3\n"
		"viim.s S030, 777\n"
		"viim.s S001, 32767\n"
		"viim.s S002, -32768\n"
		"viim.s S003, -0\n"
		"viim.s S011, -8\n"
		"viim.s S021, -3\n"
		"viim.s S031, -1\n"
		"sv.q   R000, 0x00+%0\n"
		"sv.q   R001, 0x10+%0\n"
		"sv.q   R002, 0x20+%0\n"
		"sv.q   R003, 0x30+%0\n"
		: "+m" (v[0])
	);

	for (n = 0; n < 4; n++) {
		printVector("viim", &v[n]);
	}
}

void checkVectorCopy() {
	initValues();
	vcopy(&v0, &v1);
	printVector("vmov", &v0);
}

void checkVfim() {
	initValues();
	vfim(&v0);
	printVector("vfim", &v0);
}

void checkDot() {
	initValues();
	vdotq(&v0, &v1, &v2);
	printf("vdot %f\n", v0.x);
}

void checkScale() {
	initValues();
	vsclq(&v0, &v1, &v2);
	printVector("vsql", &v0);
}

void NOINLINE vrot(float angle, ScePspFVector4 *v0) {
	asm volatile (
		"mtv    %1, s601\n"
		"vrot.p	r500, s601, [c, s]\n"
		"vrot.p	r520, s601, [c, s]\n"
		"sv.q   r500, %0\n"
		: "+m" (*v0) : "r" (angle)
		);
}

void checkRotation() {
	initValues();
	vrot(0.7, &v0);
	printVector("vrot", &v0);
}

void moveNormalRegister() {
	float t = 5.0;
	//int t2 = *(int *)&t;
	static ScePspFVector4 v[1];
	asm volatile(
		"mtv %1, S410\n"
		"mtv %1, S411\n"
		"mtv %1, S412\n"
		"mtv %1, S413\n"
		"sv.q   C410, 0x00+%0\n"
		: "+m" (v[0]) : "t" (t)
	);
	printVector("moveNormalRegister", &v0);
}

void _checkLoadUnaligned1(ScePspFVector4* v0, int index, int column) {
	float list[64] = {0.0f};
	float *vec = &list[index];
	int n;
	for (n = 0; n < 64; n++) {
		list[n] = -(float)n;
	}

	if (column) {
		__asm__ volatile (
			"vmov.q C000, R000[0, 0, 0, 0]\n"
			"lvl.q C000, %1\n"
			"sv.q   C000, 0x00+%0\n"
			: "+m" (*v0) : "m" (*vec)
		);
	} else {
		__asm__ volatile (
			"vmov.q R000, R000[0, 0, 0, 0]\n"
			"lvl.q  R000, %1\n"
			"sv.q   R000, 0x00+%0\n"
			: "+m" (*v0) : "m" (*vec)
		);
	}
}

void _checkLoadUnaligned2(ScePspFVector4* v0, int index, int column) {
	float list[64] = {0.0f};
	float *vec = &list[index];
	int n;
	for (n = 0; n < 64; n++) {
		list[n] = -(float)n;
	}

	if (column) {
		__asm__ volatile (
			"vmov.q C000, R000[0, 0, 0, 0]\n"
			"lvr.q C000, %1\n"
			"sv.q   C000, 0x00+%0\n"
			: "+m" (*v0) : "m" (*vec)
		);
	} else {
		__asm__ volatile (
			"vmov.q R000, R000[0, 0, 0, 0]\n"
			"lvr.q  R000, %1\n"
			"sv.q   R000, 0x00+%0\n"
			: "+m" (*v0) : "m" (*vec)
		);
	}
}

void checkLoadUnaligned() {
	_checkLoadUnaligned1(&v0, 13, 0);
	printVector("lvl_row", &v0);
	_checkLoadUnaligned1(&v0, 24, 0);
	printVector("lvl_row", &v0);
	_checkLoadUnaligned1(&v0, 32, 0);
	printVector("lvl_row", &v0);

	_checkLoadUnaligned1(&v0, 15, 1);
	printVector("lvl_column", &v0);
	_checkLoadUnaligned1(&v0, 23, 1);
	printVector("lvl_column", &v0);
	_checkLoadUnaligned1(&v0, 31, 1);
	printVector("lvl_column", &v0);
	
	_checkLoadUnaligned2(&v0, 13, 0);
	printVector("lvr_row", &v0);
	_checkLoadUnaligned2(&v0, 24, 0);
	printVector("lvr_row", &v0);
	_checkLoadUnaligned2(&v0, 32, 0);
	printVector("lvr_row", &v0);

	_checkLoadUnaligned2(&v0, 15, 1);
	printVector("lvr_column", &v0);
	_checkLoadUnaligned2(&v0, 23, 1);
	printVector("lvr_column", &v0);
	_checkLoadUnaligned2(&v0, 31, 1);
	printVector("lvr_column", &v0);

}

void checkVzero() {
	v0.w = v0.z = v0.y = v0.x = -1.0f;
	vzero_q(&v0);
	printVector("checkVZero", &v0);
}

void checkVone() {
	v0.w = v0.z = v0.y = v0.x = -1.0f;
	vone_q(&v0);
	printVector("checkVone", &v0);
}

void checkMisc() {
	float fovy = 75.0f;
	
	ALIGN16 ScePspFVector4 v;
	ScePspFVector4 *v0 = &v;
	
	resetAllMatrices();
	
	__asm__ volatile (
    "vmzero.q M100\n"                   // set M100 to all zeros
    "mtv     %1, S000\n"                // S000 = fovy
    "viim.s  S001, 90\n"                // S002 = 90.0f
    "vrcp.s  S001, S001\n"              // S002 = 1/90
    "vmul.s  S000, S000, S000[1/2]\n"   // S000 = fovy * 0.5 = fovy/2
    "vmul.s  S000, S000, S001\n"        // S000 = (fovy/2)/90
		"sv.q   C000, %0\n"
		: "+m" (*v0) : "r"(fovy)
	);
	
	printVector("misc", v0);
}

void NOINLINE _checkSimpleLoad(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.s   S000, 0x00+%1\n"
		"sv.s   S000, 0x00+%0\n"
		: "+m" (*v0) : "m" (*v1)
	);
}

void NOINLINE checkSimpleLoad() {
	ALIGN16 ScePspFVector4 vIn = {0.0f, 0.0f, 0.0f, 0.0f};
	ALIGN16 ScePspFVector4 vOut = {0.0f, 0.0f, 0.0f, 0.0f};
	vIn.x = 0.3f;
	_checkSimpleLoad(&vOut, &vIn);
	printVector("simpleLoad", &vOut);
}

void NOINLINE vfad_q(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	
	asm volatile (
		"lv.q   C100, %1\n"
		"vfad.q S000, C100\n"
		"sv.s   S000, 0x00+%0\n"

		: "+m" (*v0) : "m" (*v1)
	);
}

void NOINLINE vavg_q(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vavg.q S000, C100\n"
		"sv.s   S000, 0x00+%0\n"

		: "+m" (*v0) : "m" (*v1)
	);
}

void NOINLINE vavg_q3(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vavg.t S000, C100\n"
		"sv.s   S000, 0x00+%0\n"

		: "+m" (*v0) : "m" (*v1)
	);
}

void checkAggregated() {
	static ALIGN16 ScePspFVector4 vIn = {11.0f, 22.0f, 33.0f, 44.0f};
	static ALIGN16 ScePspFVector4 vIn2 = {11.0f, 22.0f, INFINITY, 44.0f};
	static ALIGN16 ScePspFVector4 vOut = {0.0f, 0.0f, 0.0f, 0.0f};
	vfad_q(&vOut, &vIn);
	printf("SUM: %f\n", vOut.x);
	vavg_q(&vOut, &vIn);
	printf("AVG: %f\n", vOut.x);
	vavg_q3(&vOut, &vIn2);
	printf("AVG3: %f\n", vOut.x);
}

void checkCrossProduct() {
	static ALIGN16 ScePspFVector4 vleft = { -1.0f, -2.0f, 3.0f, 4.0f};
	static ALIGN16 ScePspFVector4 vright = { -3.0f, 5.0f, 7.0f, -11.0f };
	static ALIGN16 ScePspFVector4 vout = { 0.0f, 0.0f, 0.0f, 0.0f };
	vcsrp_t(&vleft, &vright, &vout);
	printVector("vcrsp_t", &vout);
	vqmul_q(&vleft, &vright, &vout);
	printVector("vqmul_q", &vout);
}

void NOINLINE vcrs_t(ScePspFVector4 *vleft, ScePspFVector4 *vright, ScePspFVector4 *vresult) {
	asm volatile (
		"lv.q R500, 0x00+%1\n"
		"lv.q R600, 0x00+%2\n"
		
		"vcrs.t R100, R500, R600\n"
		
		"sv.q    R100, 0x00+%0\n"
		: "+m" (*vresult) : "m" (*vleft), "m" (*vright)
	);
}

void checkHalfCrossProduct() {
	static ALIGN16 ScePspFVector4 vleft = { -1.0f, -2.0f, 3.0f, 4.0f};
	static ALIGN16 ScePspFVector4 vright = { -3.0f, 5.0f, 7.0f, -11.0f };
	static ALIGN16 ScePspFVector4 vout = { 0.0f, 0.0f, 0.0f, 0.0f };
	vcrs_t(&vleft, &vright, &vout);
	printVector("vcrs_t", &vout);
}

void NOINLINE _checkCompare(ScePspFVector4 *vleft, ScePspFVector4 *vright, ScePspFVector4 *vresult) {
	asm volatile (
		"lv.q R500, 0x00+%1\n"
		"lv.q R600, 0x00+%2\n"
		
		"vcmp.t	EQ,  R500, R600\n"
		
		"vfim.s	  S100, -0\n"
		"vfim.s	  S110, -0\n"
		"vfim.s	  S120, -0\n"
		"vfim.s	  S130, -0\n"
		
		"vcmovt.s S100, S000[1], 0\n"
		"vcmovt.s S110, S000[1], 1\n"
		"vcmovt.s S120, S000[1], 2\n"
		"vcmovt.s S130, S000[1], 3\n"
		
		"sv.q    R100, 0x00+%0\n"
		: "+m" (*vresult) : "m" (*vleft), "m" (*vright)
	);
}

void checkCompare() {
	static ALIGN16 ScePspFVector4 vleft  = { 1.0f, -1.0f, -1.1f, 2.0f };
	static ALIGN16 ScePspFVector4 vright = { 1.0f,  1.0f, -1.1f, 2.1f };
	static ALIGN16 ScePspFVector4 vout = { 0.0f, 0.0f, 0.0f, 0.0f };
	_checkCompare(&vleft, &vright, &vout);
	printVector("checkCompare", &vout);
}

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

void NOINLINE _checkCompare2(float a, float b) {
 	static ALIGN16 ScePspFVector4 vleft;
	static ALIGN16 ScePspFVector4 vright;
 	ScePspFVector4 *vLeft = &vleft;
 	ScePspFVector4 *vRight = &vright;
  int i;
  for (i = 0; i < 4; i++) {
    memcpy(((float*)&vleft) + i, &a, 4);
    memcpy(((float*)&vright) + i, &b, 4);
  }
  int temp;
  unsigned int res[16];
  printf("checkCompare2: === Comparing %f, %f ===\n", a, b);
  memset(res, 0, sizeof(res));
  asm volatile(
		"lv.q R500, 0x00+%1\n"
		"lv.q R600, 0x00+%2\n"
    "li %3, 0\n"
    "mtvc %3, $131\n"

		// $131 = VFPU_CC
    // The vmul is necessary to resolve the hazard of reading VFPU_CC immediately after writing it.
    // Apparently, Sony didn't bother to implement proper interlocking.
    "vcmp.q FL, R500, R600\n" "vmul.q R700, R701, R702\n" "mfvc %3, $131\n" "sw %3, 0+%0\n"
    "vcmp.q EQ, R500, R600\n" "vmul.q R700, R701, R702\n" "mfvc %3, $131\n" "sw %3, 4+%0\n"
    "vcmp.q LT, R500, R600\n" "vmul.q R700, R701, R702\n" "mfvc %3, $131\n" "sw %3, 8+%0\n"
    "vcmp.q LE, R500, R600\n" "vmul.q R700, R701, R702\n" "mfvc %3, $131\n" "sw %3, 12+%0\n"

    "vcmp.q TR, R500, R600\n" "vmul.q R700, R701, R702\n" "mfvc %3, $131\n" "sw %3, 16+%0\n"
    "vcmp.q NE, R500, R600\n" "vmul.q R700, R701, R702\n" "mfvc %3, $131\n" "sw %3, 20+%0\n"
    "vcmp.q GE, R500, R600\n" "vmul.q R700, R701, R702\n" "mfvc %3, $131\n" "sw %3, 24+%0\n"
    "vcmp.q GT, R500, R600\n" "vmul.q R700, R701, R702\n" "mfvc %3, $131\n" "sw %3, 28+%0\n"

    "vcmp.q EZ, R500, R600\n" "vmul.q R700, R701, R702\n" "mfvc %3, $131\n" "sw %3, 32+%0\n"
    "vcmp.q EN, R500, R600\n" "vmul.q R700, R701, R702\n" "mfvc %3, $131\n" "sw %3, 36+%0\n"
    "vcmp.q EI, R500, R600\n" "vmul.q R700, R701, R702\n" "mfvc %3, $131\n" "sw %3, 40+%0\n"
    "vcmp.q ES, R500, R600\n" "vmul.q R700, R701, R702\n" "mfvc %3, $131\n" "sw %3, 44+%0\n"
    
    "vcmp.q NZ, R500, R600\n" "vmul.q R700, R701, R702\n" "mfvc %3, $131\n" "sw %3, 48+%0\n"
    "vcmp.q NN, R500, R600\n" "vmul.q R700, R701, R702\n" "mfvc %3, $131\n" "sw %3, 52+%0\n"
    "vcmp.q NI, R500, R600\n" "vmul.q R700, R701, R702\n" "mfvc %3, $131\n" "sw %3, 56+%0\n"
    "vcmp.q NS, R500, R600\n" "vmul.q R700, R701, R702\n" "mfvc %3, $131\n" "sw %3, 60+%0\n"
    : "=m"(res[0]) : "m"(*vLeft), "m"(*vRight), "r"(temp)
  );
  for (i = 0; i < 16; i++) {
    if (0) {
      //simple mode, only condition flag
      printf("checkCompare2: %f %f %s: %s\n", a, b, cmpNames[i], ((res[i]>>23)&1) ? "T" : "F");
    } else {
      printf("checkCompare2: %f %f %s: %08x\n", a, b, cmpNames[i], res[i]);
    }
  }
}

void checkCompare2() {
	_checkCompare2(1.0f, 2.0f);
  _checkCompare2(2.0f, 1.0f);
	_checkCompare2(-2.0f, -1.0f);
  _checkCompare2(2.0f, 2.0f);
	_checkCompare2(0.0f, -0.0f);
	_checkCompare2(INFINITY, INFINITY);
  _checkCompare2(-INFINITY, 100.0f);
  _checkCompare2(NAN, 0.0f);
  _checkCompare2(0.0f, NAN);
  _checkCompare2(NAN, NAN);
  _checkCompare2(NAN, -NAN);
  _checkCompare2(-NAN, -NAN);
  _checkCompare2(-0.0f, 0.0f);
	_checkCompare2(NAN, INFINITY);
	_checkCompare2(INFINITY, NAN);
}


int main(int argc, char *argv[]) {
	printf("Started\n");

	resetAllMatrices();
	
	checkCompare();
	checkCompare2();
	checkCrossProduct();
	checkHalfCrossProduct();
	checkVbfy1();
	checkVbfy2();
	checkAggregated();
	checkSimpleLoad();
	checkMisc();
	checkVmin();
	checkVmax();
	checkVsge();
	checkVslt();
	checkVadd();
	checkVsub();
	checkVdiv();
	checkVmul();
	checkVrcp();
	checkViim();
  checkVsgn();
  checkVsat0();
	checkLoadUnaligned();
	moveNormalRegister();
	checkVfim();
	checkConstants();
	checkVectorCopy();
	checkDot();
	checkScale();
	checkRotation();
	checkVzero();
	checkVone();

	printf("Ended\n");
	return 0;
}
