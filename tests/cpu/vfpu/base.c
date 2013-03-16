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
//#include "../common/emits.h"

#include <pspgu.h>
#include <pspgum.h>


__attribute__ ((aligned (16))) ScePspFVector4 v0, v1, v2;
__attribute__ ((aligned (16))) ScePspFVector4 matrix[4];

void resetAllMatrices() {
	asm volatile (
		"vmzero.q  M000\n"
		"vmzero.q  M100\n"
		"vmzero.q  M200\n"
		"vmzero.q  M300\n"
		"vmzero.q  M400\n"
		"vmzero.q  M500\n"
		"vmzero.q  M600\n"
		"vmzero.q  M700\n"
	);
}

void __attribute__((noinline)) vcopy(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C720, %1\n"
		"sv.q   C720, %0\n"

		: "+m" (*v0) : "m" (*v1)
	);
}

void __attribute__((noinline)) vf2h(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vf2h.q C200, C100\n"
		"sv.q   C200, %0\n"

		: "+m" (*v0) : "m" (*v1)
	);
}

void __attribute__((noinline)) vh2f(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vh2f.p C200, C100\n"
		"sv.q   C200, %0\n"

		: "+m" (*v0) : "m" (*v1)
	);
}

void __attribute__((noinline)) vf2id(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vf2id.q C200, C100, 0\n"
		"sv.q   C200, %0\n"

		: "+m" (*v0) : "m" (*v1)
		);
}

void __attribute__((noinline)) vf2in(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vf2in.q C200, C100, 0\n"
		"sv.q   C200, %0\n"

		: "+m" (*v0) : "m" (*v1)
		);
}

void __attribute__((noinline)) vf2iu(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vf2iu.q C200, C100, 0\n"
		"sv.q   C200, %0\n"

		: "+m" (*v0) : "m" (*v1)
		);
}

void __attribute__((noinline)) vf2iz(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vf2iz.q C200, C100, 0\n"
		"sv.q   C200, %0\n"

		: "+m" (*v0) : "m" (*v1)
		);
}

void __attribute__((noinline)) vi2f(ScePspFVector4 *v0, int *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vi2f.q C200, C100, 0\n"
		"sv.q   C200, %0\n"

		: "+m" (*v0) : "m" (*v1)
		);
}



void __attribute__((noinline)) vrot(float angle, ScePspFVector4 *v0) {
	asm volatile (
		"mtv    %1, s501\n"

		"vrot.p	r500, s501, [c, s]\n"
		
		"sv.q   r500, %0\n"

		: "+m" (*v0) : "r" (angle)
	);
}

void __attribute__((noinline)) vdotq(ScePspFVector4 *v0, ScePspFVector4 *v1, ScePspFVector4 *v2) {
	asm volatile (
		"lv.q   C100, %1\n"
		"lv.q   C200, %2\n"
		"vdot.q S000, C100, C200\n"
		"sv.q   C000, %0\n"

		: "+m" (*v0) : "m" (*v1), "m" (*v2)
	);
}

void __attribute__((noinline)) vsclq(ScePspFVector4 *v0, ScePspFVector4 *v1, ScePspFVector4 *v2) {
	asm volatile (
		"lv.q   C100, %1\n"
		"lv.q   C200, %2\n"
		"vscl.q C300, C100, S200\n"
		"sv.q   C300, %0\n"

		: "+m" (*v0) : "m" (*v1), "m" (*v2)
	);
}

void __attribute__((noinline)) vmidt(int size, ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q    R000, %1\n"
		"lv.q    R001, %1\n"
		"lv.q    R002, %1\n"
		"lv.q    R003, %1\n"

		: "+m" (*v0) : "m" (*v1)
	);
	
	switch (size) {
		case 2: asm volatile("vmidt.p M000\n"); break;
		case 3: asm volatile("vmidt.t M000\n"); break;
		case 4: asm volatile("vmidt.q M000\n"); break;
	}

	asm volatile (
		"sv.q    R000, 0x00+%0\n"
		"sv.q    R001, 0x10+%0\n"
		"sv.q    R002, 0x20+%0\n"
		"sv.q    R003, 0x30+%0\n"

		: "+m" (*v0) : "m" (*v1)
	);
}

void __attribute__((noinline)) vfim(ScePspFVector4 *v0) {
	asm volatile (
		"vfim.s	 s500, 0.011111111111111112\n"
		"vfim.s	 s501, -0.011111111111111112\n"
		"vfim.s	 s502, inf\n"
		"vfim.s	 s503, nan\n"
		"sv.q    C500, %0\n"

		: "+m" (*v0)
	);
}


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

void checkMatrixIdentity() {
	int vsize, x, y;
	ScePspFVector4 matrix2[4];
	for (vsize = 2; vsize <= 4; vsize++) {
		v0.x = 100;
		v0.y = 101;
		v0.z = 102;
		v0.w = 103;
		vmidt(vsize, &matrix[0], &v0);
		for (y = 0; y < 4; y++) {
			matrix2[y] = v0;
			for (x = 0; x < 4; x++) {
				if (x < vsize && y < vsize) {
					((float *)&matrix2[y])[x] = (float)(x == y);
				}
			}
		}
		/*
		printf("-------------------\n");
		for (y = 0; y < 4; y++) printf("(%3.0f, %3.0f, %3.0f, %3.0f)\n", matrix[y].x, matrix[y].y, matrix[y].z, matrix[y].w);
		printf("+\n");
		for (y = 0; y < 4; y++) printf("(%3.0f, %3.0f, %3.0f, %3.0f)\n", matrix2[y].x, matrix2[y].y, matrix2[y].z, matrix2[y].w);
		printf("\n");
		*/
		printf("%d\n", memcmp((void *)&matrix, (void *)&matrix2, sizeof(matrix)));
	}
	
	//printf("Test! %f, %f, %f, %f, %f, %f, %f, %f, %f, %f\n", 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0);
	//printf("Test! %d, %d, %d, %d\n", 1, 2, 3, -3);
}

void checkConstants() {
	static ScePspFVector4 v[5];
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
	char buf[1024];
	char temp[1024];
	buf[0] = 0;
	for (n = 0; n < 5; n++) {
		sprintf(temp, "%f,%f,%f,%f\n", v[n].x, v[n].y, v[n].z, v[n].w);
		strcat(buf, temp);
	}
	//printf("%s", buf);
	printf("checkConstants(Comparison): %s\n", (strcmp(buf,
		"340282346638528859811704183484516925440.000000,1.414214,0.707107,1.128379\n"
		"0.636620,0.318310,0.785398,1.570796\n"
		"3.141593,2.718282,1.442695,0.434294\n"
		"0.693147,2.302585,6.283185,0.523599\n"
		"0.301030,3.321928,0.866025,0.000000\n"
	) == 0) ? "Ok" : "ERROR");
	
	puts(buf);
}


void checkVF2I() {
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn1 =
	{0.9f, 1.3f, 2.7f, 1000.5f};
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn2 =
	{-0.9f, -1.3f, -2.7f, -1000.5f};
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn3 =
	{1.5f, 2.5f, -3.5f, -4.5f};
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn4 =
	{3.5f, INFINITY, -INFINITY, NAN};


	static __attribute__ ((aligned (16))) ScePspFVector4 vOutF =
	{0.0f, 0.0f, 0.0f, 0.0f};

	struct {int x,y,z,w;} vOut;

	vf2id(&vOutF, &vIn1); memcpy(&vOut, &vOutF, 16);
	printf("vf2id: %i,%i,%i,%i\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vf2id(&vOutF, &vIn2); memcpy(&vOut, &vOutF, 16);
	printf("vf2id: %i,%i,%i,%i\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vf2id(&vOutF, &vIn3); memcpy(&vOut, &vOutF, 16);
	printf("vf2id: %i,%i,%i,%i\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vf2id(&vOutF, &vIn4); memcpy(&vOut, &vOutF, 16);
	printf("vf2id: %i,%i,%i,%i\n", vOut.x, vOut.y, vOut.z, vOut.w);

	vf2in(&vOutF, &vIn1); memcpy(&vOut, &vOutF, 16);
	printf("vf2in: %i,%i,%i,%i\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vf2in(&vOutF, &vIn2); memcpy(&vOut, &vOutF, 16);
	printf("vf2in: %i,%i,%i,%i\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vf2in(&vOutF, &vIn3); memcpy(&vOut, &vOutF, 16);
	printf("vf2in: %i,%i,%i,%i\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vf2in(&vOutF, &vIn4); memcpy(&vOut, &vOutF, 16);
	printf("vf2in: %i,%i,%i,%i\n", vOut.x, vOut.y, vOut.z, vOut.w);

	vf2iz(&vOutF, &vIn1); memcpy(&vOut, &vOutF, 16);
	printf("vf2iz: %i,%i,%i,%i\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vf2iz(&vOutF, &vIn2); memcpy(&vOut, &vOutF, 16);
	printf("vf2iz: %i,%i,%i,%i\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vf2iz(&vOutF, &vIn3); memcpy(&vOut, &vOutF, 16);
	printf("vf2iz: %i,%i,%i,%i\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vf2iz(&vOutF, &vIn4); memcpy(&vOut, &vOutF, 16);
	printf("vf2iz: %i,%i,%i,%i\n", vOut.x, vOut.y, vOut.z, vOut.w);

	vf2iu(&vOutF, &vIn1); memcpy(&vOut, &vOutF, 16);
	printf("vf2iu: %i,%i,%i,%i\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vf2iu(&vOutF, &vIn2); memcpy(&vOut, &vOutF, 16);
	printf("vf2iu: %i,%i,%i,%i\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vf2iu(&vOutF, &vIn3); memcpy(&vOut, &vOutF, 16);
	printf("vf2iu: %i,%i,%i,%i\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vf2iu(&vOutF, &vIn4); memcpy(&vOut, &vOutF, 16);
	printf("vf2iu: %i,%i,%i,%i\n", vOut.x, vOut.y, vOut.z, vOut.w);
}

void checkVI2F() {
	static __attribute__ ((aligned (16))) int vIn1[4] =
	{0, 0xFFFFFFFF, 3, 0x80000000};
	static __attribute__ ((aligned (16))) int vIn2[4] =
	{-1, -2, -3, 0x10000};

	static __attribute__ ((aligned (16))) ScePspFVector4 vOut =
	{0.0f, 0.0f, 0.0f, 0.0f};

	vi2f(&vOut, vIn1);
	printf("vi2f: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vi2f(&vOut, vIn2);
	printf("vi2f: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
}

void checkHalf() {
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn1 =
	{0.9f, 1.3f, 2.7f, 1000.5f};
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn2 =
	{-0.9f, -1.3f, -2.7f, -1000.5f};
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn3 =
	{1.5f, 2.5f, -3.5f, -4.5f};
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn4 =
	{3.5f, INFINITY, -INFINITY, NAN};

	static __attribute__ ((aligned (16))) ScePspFVector4 vOutF =
	{0.0f, 0.0f, 0.0f, 0.0f};

	struct {unsigned int x,y,z,w;} vOut;

	vf2h(&vOutF, &vIn1); memcpy(&vOut, &vOutF, 16);
	printf("vf2h: %08x,%08x,%08x,%08x\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vf2h(&vOutF, &vIn2); memcpy(&vOut, &vOutF, 16);
	printf("vf2h: %08x,%08x,%08x,%08x\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vf2h(&vOutF, &vIn3); memcpy(&vOut, &vOutF, 16);
	printf("vf2h: %08x,%08x,%08x,%08x\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vf2h(&vOutF, &vIn4); memcpy(&vOut, &vOutF, 16);
	printf("vf2h: %08x,%08x,%08x,%08x\n", vOut.x, vOut.y, vOut.z, vOut.w);

	vh2f(&vOutF, &vIn1); memcpy(&vOut, &vOutF, 16);
	printf("vh2f: %08x,%08x,%08x,%08x\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vh2f(&vOutF, &vIn2); memcpy(&vOut, &vOutF, 16);
	printf("vh2f: %08x,%08x,%08x,%08x\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vh2f(&vOutF, &vIn3); memcpy(&vOut, &vOutF, 16);
	printf("vh2f: %08x,%08x,%08x,%08x\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vh2f(&vOutF, &vIn4); memcpy(&vOut, &vOutF, 16);
	printf("vh2f: %08x,%08x,%08x,%08x\n", vOut.x, vOut.y, vOut.z, vOut.w);

	vf2h(&vOutF, &vIn1); vh2f(&vOutF, &vOutF);
	printf("vf2h vh2f: %f,%f,%f,%f\n", vOutF.x, vOutF.y, vOutF.z, vOutF.w);
	vf2h(&vOutF, &vIn2); vh2f(&vOutF, &vOutF);
	printf("vf2h vh2f: %f,%f,%f,%f\n", vOutF.x, vOutF.y, vOutF.z, vOutF.w);
	vf2h(&vOutF, &vIn3); vh2f(&vOutF, &vOutF);
	printf("vf2h vh2f: %f,%f,%f,%f\n", vOutF.x, vOutF.y, vOutF.z, vOutF.w);
	vf2h(&vOutF, &vIn4); vh2f(&vOutF, &vOutF);
	printf("vf2h vh2f: %f,%f,%f,%f\n", vOutF.x, vOutF.y, vOutF.z, vOutF.w);
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

void __attribute__((noinline)) vsgn(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vsgn.q C200, C100\n"
		"sv.q   C200, %0\n"

		: "+m" (*v0) : "m" (*v1)
		);
}

void checkVsgn() {
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn =
	{-0.0f, 1.3f, -INFINITY, NAN};
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn2 =
	{0.0f, -1.0f, INFINITY, -NAN};
	static __attribute__ ((aligned (16))) ScePspFVector4 vOut =
	{0.0f, 0.0f, 0.0f, 0.0f};

	vsgn(&vOut, &vIn);
	printf("vsgn.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vsgn(&vOut, &vIn2);
	printf("vsgn.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
}

void __attribute__((noinline)) vrcp(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vrcp.q C200, C100\n"
		"sv.q   C200, %0\n"

		: "+m" (*v0) : "m" (*v1)
		);
}

void checkVrcp() {
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn =
	{0.0f, -1.3f, INFINITY, NAN};
	static __attribute__ ((aligned (16))) ScePspFVector4 vOut =
	{0.0f, 0.0f, 0.0f, 0.0f};

	vrcp(&vOut, &vIn);
	printf("vrcp.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
}

void __attribute__((noinline)) vbfy1(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vbfy1.q C200, C100\n"
		"sv.q   C200, %0\n"

		: "+m" (*v0) : "m" (*v1)
		);
}

void checkVbfy1() {
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn =
	{10.0f, -5.f, 7.f, 3.f};
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn2 =
	{6.0f, -46.f, 7.f, -INFINITY};
	static __attribute__ ((aligned (16))) ScePspFVector4 vOut =
	{0.0f, 0.0f, 0.0f, 0.0f};

	vbfy1(&vOut, &vIn);
	printf("vbfy1.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vbfy1(&vOut, &vIn2);
	printf("vbfy1.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
}

void __attribute__((noinline)) vbfy2(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vbfy2.q C200, C100\n"
		"sv.q   C200, %0\n"

		: "+m" (*v0) : "m" (*v1)
		);
}

void checkVbfy2() {
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn =
	{20.0f, -1.f, 4.f, 9.f};
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn2 =
	{11.0f, -3.f, 8.f, NAN};
	static __attribute__ ((aligned (16))) ScePspFVector4 vOut =
	{0.0f, 0.0f, 0.0f, 0.0f};

	vbfy2(&vOut, &vIn);
	printf("vbfy2.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vbfy2(&vOut, &vIn2);
	printf("vbfy2.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
}

void __attribute__((noinline)) vsat0(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vsat0.q C200, C100\n"
		"sv.q   C200, %0\n"

		: "+m" (*v0) : "m" (*v1)
		);
}

void checkVsat0() {
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn =
	{0.0f, -1.3f, INFINITY, NAN};
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn2 =
	{10e10f, 1.000001f, -INFINITY, -NAN};
	static __attribute__ ((aligned (16))) ScePspFVector4 vOut =
	{0.0f, 0.0f, 0.0f, 0.0f};

	vsat0(&vOut, &vIn);
	printf("vsat0.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
	vsat0(&vOut, &vIn2);
	printf("vsat0.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
}

void __attribute__((noinline)) vmin(ScePspFVector4 *v0, ScePspFVector4 *v1, ScePspFVector4 *v2) {
	asm volatile (
		"lv.q   C100, %1\n"
		"lv.q   C200, %2\n"
		"vmin.q C300, C100, C200\n"
		"sv.q   C300, %0\n"

		: "+m" (*v0): "m" (*v1), "m" (*v2)
		);
}

void checkVmin() {
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn =
	{3.0f, -4.0f, NAN, 86.0f};
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn2 =
	{4.0f, -5.0f, 0.0f, NAN};
	static __attribute__ ((aligned (16))) ScePspFVector4 vOut =
	{0.0f, 0.0f, 0.0f, 0.0f};

	vmin(&vOut, &vIn, &vIn2);
	printf("vmin.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
}

void __attribute__((noinline)) vmax(ScePspFVector4 *v0, ScePspFVector4 *v1, ScePspFVector4 *v2) {
	asm volatile (
		"lv.q   C100, %1\n"
		"lv.q   C200, %2\n"
		"vmax.q C300, C200, C100\n"
		"sv.q   C300, %0\n"

		: "+m" (*v0): "m" (*v1), "m" (*v2)
		);
}

void checkVmax() {
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn =
	{3.0f, -4.0f, NAN, 86.0f};
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn2 =
	{4.0f, -5.0f, 0.0f, NAN};
	static __attribute__ ((aligned (16))) ScePspFVector4 vOut =
	{0.0f, 0.0f, 0.0f, 0.0f};

	vmax(&vOut, &vIn, &vIn2);
	printf("vmax.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
}

void __attribute__((noinline)) vsge(ScePspFVector4 *v0, ScePspFVector4 *v1, ScePspFVector4 *v2) {
	asm volatile (
		"lv.q   C100, %1\n"
		"lv.q   C200, %2\n"
		"vsge.q C300, C200, C100\n"
		"sv.q   C300, %0\n"

		: "+m" (*v0): "m" (*v1), "m" (*v2)
		);
}

void checkVsge() {
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn =
	{3.0f, -4.0f, NAN, 86.0f};
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn2 =
	{4.0f, -5.0f, 0.0f, NAN};
	static __attribute__ ((aligned (16))) ScePspFVector4 vOut =
	{0.0f, 0.0f, 0.0f, 0.0f};

	vsge(&vOut, &vIn, &vIn2);
	printf("vsge.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
}

void __attribute__((noinline)) vslt(ScePspFVector4 *v0, ScePspFVector4 *v1, ScePspFVector4 *v2) {
	asm volatile (
		"lv.q   C100, %1\n"
		"lv.q   C200, %2\n"
		"vslt.q C300, C200, C100\n"
		"sv.q   C300, %0\n"

		: "+m" (*v0): "m" (*v1), "m" (*v2)
		);
}

void checkVslt() {
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn =
	{3.0f, -4.0f, NAN, 86.0f};
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn2 =
	{4.0f, -5.0f, 0.0f, NAN};
	static __attribute__ ((aligned (16))) ScePspFVector4 vOut =
	{0.0f, 0.0f, 0.0f, 0.0f};

	vslt(&vOut, &vIn, &vIn2);
	printf("vslt.q: %f,%f,%f,%f\n", vOut.x, vOut.y, vOut.z, vOut.w);
}

void checkViim() {
	int n;
	static ScePspFVector4 v[5];

	asm volatile(
		"viim.s S000, 0\n"
		"viim.s S010, 1\n"
		"viim.s S020, -3\n"
		"viim.s S030, 777\n"
		"viim.s S001, 32767\n"
		"viim.s S011, -8\n"
		"viim.s S021, -3\n"
		"viim.s S031, -1\n"
		"sv.q   R000, 0x00+%0\n"
		"sv.q   R001, 0x10+%0\n"
		: "+m" (v[0])
	);

	for (n = 0; n < 2; n++) {
		printf("%f,%f,%f,%f\n", v[n].x, v[n].y, v[n].z, v[n].w);
	}
}

void checkVectorCopy() {
	initValues();
	vcopy(&v0, &v1);
	printf("%f, %f, %f, %f\n", v0.x, v0.y, v0.z, v0.w);
}

void checkVfim() {
	initValues();
	vfim(&v0);
	printf("%f, %f, %f, %f\n", v0.x, v0.y, v0.z, v0.w);
}

void checkDot() {
	initValues();
	vdotq(&v0, &v1, &v2);
	printf("%f\n", v0.x);
}

void checkScale() {
	initValues();
	vsclq(&v0, &v1, &v2);
	printf("%f, %f, %f, %f\n", v0.x, v0.y, v0.z, v0.w);
}

void checkRotation() {
	initValues();
	vrot(0.7, &v0);
	// HACK: TOLERANCE
	printf("%0.5f, %0.5f\n", v0.x, v0.y);
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
	printf("%f, %f, %f, %f\n", v[0].x, v[0].y, v[0].z, v[0].w);
}

void _checkMultiply2(ScePspFMatrix4* m0, ScePspFMatrix4 *m1, ScePspFMatrix4 *m2) {
	__asm__ volatile (

		"lv.q    R000, 0x00+%1\n"
		"lv.q    R001, 0x10+%1\n"
		"lv.q    R002, 0x20+%1\n"
		"lv.q    R003, 0x30+%1\n"

    "lv.q    R100, 0x00+%2\n"
		"lv.q    R101, 0x10+%2\n"
		"lv.q    R102, 0x20+%2\n"
		"lv.q    R103, 0x30+%2\n"

		"vmmul.p M200, M000, M100\n"
		"vmmul.p M202, M002, E102\n"
		"vmmul.p E220, M020, M120\n"
		"vmmul.p M222, E022, M122\n"

		"sv.q    R200, 0x00+%0\n"
		"sv.q    R201, 0x10+%0\n"
		"sv.q    R202, 0x20+%0\n"
		"sv.q    R203, 0x30+%0\n"
		: "+m" (*m0) : "m" (*m1), "m" (*m2)
	);
}

void _checkMultiply4(ScePspFMatrix4* m0, ScePspFMatrix4 *m1, ScePspFMatrix4 *m2) {
	__asm__ volatile (

		"lv.q    R000, 0x00+%1\n"
		"lv.q    R001, 0x10+%1\n"
		"lv.q    R002, 0x20+%1\n"
		"lv.q    R003, 0x30+%1\n"

    "lv.q    R100, 0x00+%2\n"
		"lv.q    R101, 0x10+%2\n"
		"lv.q    R102, 0x20+%2\n"
		"lv.q    R103, 0x30+%2\n"

		"vmmul.q M200, M000, M100\n"

		"sv.q    R200, 0x00+%0\n"
		"sv.q    R201, 0x10+%0\n"
		"sv.q    R202, 0x20+%0\n"
		"sv.q    R203, 0x30+%0\n"
		: "+m" (*m0) : "m" (*m1), "m" (*m2)
	);
}

void checkMultiply() {
	int n;
	static ScePspFMatrix4 matrix1 = {
		{ 1.0f, 5.0f,  9.0f, 13.0f },
		{ 2.0f, 6.0f, 10.0f, 14.0f },
		{ 3.0f, 7.0f, 11.0f, 15.0f },
		{ 4.0f, 8.0f, 12.0f, 16.0f }
	};
	static ScePspFMatrix4 matrix2 = {
		{ -1.0f, 20.0f, 30.0f, -4.0f },
		{ 10.0f, 2.0f, 30000.0f, 400000.0f },
		{ 100.0f, 200.0f, -3.0f, 40.0f },
		{ -1000.0f, 2.0f, -0.3f, -400.0f }
	};

  printf("Multiply2:\n");
	_checkMultiply2(&matrix[0], &matrix1, &matrix2);
	for (n = 0; n < 4; n++) {
		printf("%f, %f, %f, %f\n", matrix[n].x, matrix[n].y, matrix[n].z, matrix[n].w);
	}
  printf("Multiply4:\n");
	_checkMultiply4(&matrix[0], &matrix1, &matrix2);
	for (n = 0; n < 4; n++) {
		printf("%f, %f, %f, %f\n", matrix[n].x, matrix[n].y, matrix[n].z, matrix[n].w);
	}
}

void _checkTranspose(ScePspFMatrix4* m0, ScePspFMatrix4 *m1) {
	__asm__ volatile (

		"lv.q    R000, 0x00+%1\n"
		"lv.q    R001, 0x10+%1\n"
		"lv.q    R002, 0x20+%1\n"
		"lv.q    R003, 0x30+%1\n"

		"sv.q    C000, 0x00+%0\n"
		"sv.q    C010, 0x10+%0\n"
		"sv.q    C020, 0x20+%0\n"
		"sv.q    C030, 0x30+%0\n"
		: "+m" (*m0) : "m" (*m1)
	);
}

void checkTranspose() {
	int n;
	static ScePspFMatrix4 matrix1 = {
		{ 1.0f, 5.0f,  9.0f, 13.0f },
		{ 2.0f, 6.0f, 10.0f, 14.0f },
		{ 3.0f, 7.0f, 11.0f, 15.0f },
		{ 4.0f, 8.0f, 12.0f, 16.0f }
	};

  printf("Transpose:\n");
	_checkTranspose(&matrix[0], &matrix1);
	for (n = 0; n < 4; n++) {
		printf("%f, %f, %f, %f\n", matrix[n].x, matrix[n].y, matrix[n].z, matrix[n].w);
	}
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
	printf(" lvl_row:\n");
	_checkLoadUnaligned1(&v0, 13, 0);
	printf("%f, %f, %f, %f\n", v0.x, v0.y, v0.z, v0.w);
	_checkLoadUnaligned1(&v0, 24, 0);
	printf("%f, %f, %f, %f\n", v0.x, v0.y, v0.z, v0.w);
	_checkLoadUnaligned1(&v0, 32, 0);
	printf("%f, %f, %f, %f\n", v0.x, v0.y, v0.z, v0.w);

	printf(" lvl_column:\n");
	_checkLoadUnaligned1(&v0, 15, 1);
	printf("%f, %f, %f, %f\n", v0.x, v0.y, v0.z, v0.w);
	_checkLoadUnaligned1(&v0, 23, 1);
	printf("%f, %f, %f, %f\n", v0.x, v0.y, v0.z, v0.w);
	_checkLoadUnaligned1(&v0, 31, 1);
	printf("%f, %f, %f, %f\n", v0.x, v0.y, v0.z, v0.w);
	

	printf(" lvr_row:\n");
	_checkLoadUnaligned2(&v0, 13, 0);
	printf("%f, %f, %f, %f\n", v0.x, v0.y, v0.z, v0.w);
	_checkLoadUnaligned2(&v0, 24, 0);
	printf("%f, %f, %f, %f\n", v0.x, v0.y, v0.z, v0.w);
	_checkLoadUnaligned2(&v0, 32, 0);
	printf("%f, %f, %f, %f\n", v0.x, v0.y, v0.z, v0.w);

	printf(" lvr_column:\n");
	_checkLoadUnaligned2(&v0, 15, 1);
	printf("%f, %f, %f, %f\n", v0.x, v0.y, v0.z, v0.w);
	_checkLoadUnaligned2(&v0, 23, 1);
	printf("%f, %f, %f, %f\n", v0.x, v0.y, v0.z, v0.w);
	_checkLoadUnaligned2(&v0, 31, 1);
	printf("%f, %f, %f, %f\n", v0.x, v0.y, v0.z, v0.w);

}

void _checkVzero(ScePspFVector4* v0) {
	__asm__ volatile (
		"vzero.q R000\n"
		"sv.q   R000, 0x00+%0\n"
		: "+m" (*v0)
	);
}

void checkVzero() {
	v0.w = v0.z = v0.y = v0.x = -1.0f;
	_checkVzero(&v0);
	printf("%f, %f, %f, %f\n", v0.x, v0.y, v0.z, v0.w);
}

void _checkVone(ScePspFVector4* v0) {
	__asm__ volatile (
		"vone.q R000\n"
		"sv.q   R000, 0x00+%0\n"
		: "+m" (*v0)
	);
}

void checkVone() {
	v0.w = v0.z = v0.y = v0.x = -1.0f;
	_checkVone(&v0);
	printf("%f, %f, %f, %f\n", v0.x, v0.y, v0.z, v0.w);
}

/*
int gum_current_mode = GU_PROJECTION;

int gum_matrix_update[4] = { 0 };
int gum_current_matrix_update = 0;

ScePspFMatrix4* gum_current_matrix = gum_matrix_stack[GU_PROJECTION];

ScePspFMatrix4* gum_stack_depth[4] =
{
  gum_matrix_stack[GU_PROJECTION],
  gum_matrix_stack[GU_VIEW],
  gum_matrix_stack[GU_MODEL],
  gum_matrix_stack[GU_TEXTURE]
};

ScePspFMatrix4 gum_matrix_stack[4][32];

struct pspvfpu_context *gum_vfpucontext;
*/

void printVector(ScePspFVector4 *v) {
	printf("%f,%f,%f,%f\n", v->x, v->y, v->z, v->w);
}

void printMatrix(ScePspFMatrix4 *m) {
	printVector(&m->x);
	printVector(&m->y);
	printVector(&m->z);
	printVector(&m->w);
}

void checkGum() {
	__attribute__ ((aligned (16))) ScePspFMatrix4 m;
	int val = 45;
	float angle = 0.7f;
	float c = cosf(angle);
	float s = sinf(angle);
	__attribute__ ((aligned (16))) ScePspFVector3 pos = { 0, 0, -2.5f };
	__attribute__ ((aligned (16))) ScePspFVector3 rot = { val * 0.79f * (GU_PI/180.0f), val * 0.98f * (GU_PI/180.0f), val * 1.32f * (GU_PI/180.0f) };

	sceGuInit();

	sceGumMatrixMode(GU_PROJECTION);
	sceGumLoadIdentity();
	sceGumPerspective(75.0f,16.0f/9.0f,0.5f,1000.0f);
	sceGumStoreMatrix(&m);
	printf(" PROJ:\n");
	printMatrix(&m);

	sceGumMatrixMode(GU_VIEW);
	sceGumLoadIdentity();
	sceGumStoreMatrix(&m);
	printf(" VIEW:\n");
	printMatrix(&m);

	// Complete Rotation
	sceGumMatrixMode(GU_MODEL);
	sceGumLoadIdentity();
	sceGumTranslate(&pos);
	sceGumRotateXYZ(&rot);
	printf(" MODEL:\n");
	printf("Vec: (x=%f, y=%f, z=%f)\n", rot.x, rot.y, rot.z);
	printf("Cos/Sin (%f, %f)\n", c, s);
	sceGumStoreMatrix(&m);
	printMatrix(&m);

	// Simple Rotation
	sceGumMatrixMode(GU_MODEL);
	sceGumLoadIdentity();
	sceGumRotateX(30.0f);
	sceGumStoreMatrix(&m);
	printf(" MODEL:\n");
	printMatrix(&m);
	
	//sceGumUpdateMatrix();
}

void _checkGlRotate() {
	int n;
	float M[16]; 
	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glRotatef(180, 0, 0, 1.0);
	
	glGetFloatv(GL_MODELVIEW_MATRIX, M);
	for (n = 0; n < 4; n++) {
		printf("%f, %f, %f, %f\n", M[n * 4 + 0], M[n * 4 + 1], M[n * 4 + 2], M[n * 4 + 3]);
	}
}

void checkGlRotate(int argc, char *argv[]) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(480, 272);
	glutCreateWindow(__FILE__);
	_checkGlRotate();
}

void checkMultiplyFull() {
	static __attribute__ ((aligned (16))) ScePspFMatrix4 m1 = {
		{  2,  3,  5,  7 },
		{ 11, 13, 17, 19 },
		{ 23, 29, 31, 37 },
		{ 41, 43, 47, 53 },
	};

	static __attribute__ ((aligned (16))) ScePspFMatrix4 m2 = {
		{  59,  61,  67,  71 },
		{  73,  79,  83,  89 },
		{  97, 101, 103, 107 },
		{ 109, 113, 127, 131 },
	};
	
	static __attribute__ ((aligned (16))) ScePspFMatrix4 m3;
	
	__attribute__ ((aligned (16))) ScePspFVector4 *v0 = NULL, *v1 = NULL;

	v1 = &m1.x;

	asm volatile (
		"lv.q    R000, 0x00+%1\n"
		"lv.q    R001, 0x10+%1\n"
		"lv.q    R002, 0x20+%1\n"
		"lv.q    R003, 0x30+%1\n"

		: "+m" (*v0) : "m" (*v1)
	);


	v1 = &m2.x;

	asm volatile (
		"lv.q    R100, 0x00+%1\n"
		"lv.q    R101, 0x10+%1\n"
		"lv.q    R102, 0x20+%1\n"
		"lv.q    R103, 0x30+%1\n"

		: "+m" (*v0) : "m" (*v1)
	);
	
	v0 = &m3.x;

	asm volatile (
		"vmmul.q   M200, M000, M100\n"
		"sv.q R200, 0x00+%0\n"
		"sv.q R201, 0x10+%0\n"
		"sv.q R202, 0x20+%0\n"
		"sv.q R203, 0x30+%0\n"
		
		: "+m" (*v0)
	);
	
	printMatrix(&m3);

	/*
	asm volatile (
		"lv.q   C100, %1\n"
		"sv.q   C100, %0\n"

		: "+m" (*v0) : "m" (*v1)
	);
	*/
}

void checkMisc() {
	float fovy = 75.0f;
	
	__attribute__ ((aligned (16))) ScePspFVector4 v;
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
	
	printVector(v0);
}

void __attribute__((noinline)) _checkSimpleLoad(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.s   S000, 0x00+%1\n"
		"sv.s   S000, 0x00+%0\n"

		: "+m" (*v0) : "m" (*v1)
	);
}

void __attribute__((noinline)) checkSimpleLoad() {
	__attribute__ ((aligned (16))) ScePspFVector4 vIn = {0.0f, 0.0f, 0.0f, 0.0f};
	__attribute__ ((aligned (16))) ScePspFVector4 vOut = {0.0f, 0.0f, 0.0f, 0.0f};
	vIn.x = 0.3f;
	_checkSimpleLoad(&vOut, &vIn);
	printf("%f\n", vOut.x);
}

void __attribute__((noinline)) _checkAggregatedAdd(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	
	asm volatile (
		"lv.q   C100, %1\n"
		"vfad.q S000, C100\n"
		"sv.s   S000, 0x00+%0\n"

		: "+m" (*v0) : "m" (*v1)
	);
}

void __attribute__((noinline)) _checkAggregatedAvg(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vavg.q S000, C100\n"
		"sv.s   S000, 0x00+%0\n"

		: "+m" (*v0) : "m" (*v1)
	);
}

void __attribute__((noinline)) _checkAggregatedAvg3(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"vavg.t S000, C100\n"
		"sv.s   S000, 0x00+%0\n"

		: "+m" (*v0) : "m" (*v1)
	);
}
void checkAggregated() {
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn = {11.0f, 22.0f, 33.0f, 44.0f};
	static __attribute__ ((aligned (16))) ScePspFVector4 vIn2 = {11.0f, 22.0f, INFINITY, 44.0f};
	static __attribute__ ((aligned (16))) ScePspFVector4 vOut = {0.0f, 0.0f, 0.0f, 0.0f};
	_checkAggregatedAdd(&vOut, &vIn);
	printf("SUM: %f\n", vOut.x);
	_checkAggregatedAvg(&vOut, &vIn);
	printf("AVG: %f\n", vOut.x);
	_checkAggregatedAvg3(&vOut, &vIn2);
	printf("AVG3: %f\n", vOut.x);
}

void _checkMatrixScale(ScePspFMatrix4 *matrix) {
	asm volatile (
		"vfim.s	 S000, 00\n"
		"vfim.s	 S001, 01\n"
		"vfim.s	 S002, 02\n"
		"vfim.s	 S003, 03\n"
		"vfim.s	 S010, 10\n"
		"vfim.s	 S011, 11\n"
		"vfim.s	 S012, 12\n"
		"vfim.s	 S013, 13\n"
		"vfim.s	 S020, 20\n"
		"vfim.s	 S021, 21\n"
		"vfim.s	 S022, 22\n"
		"vfim.s	 S023, 23\n"
		"vfim.s	 S030, 30\n"
		"vfim.s	 S031, 31\n"
		"vfim.s	 S032, 32\n"
		"vfim.s	 S033, 33\n"
		"vfim.s	 S200, -1\n"
		
		"vmscl.q M100, E000, S200\n"
		
		"sv.q    R100, 0x00+%0\n"
		"sv.q    R101, 0x10+%0\n"
		"sv.q    R102, 0x20+%0\n"
		"sv.q    R103, 0x30+%0\n"
		: "+m" (*matrix)
	);
}

void checkMatrixScale() {
	static __attribute__ ((aligned (16))) ScePspFMatrix4 matrix;
	_checkMatrixScale(&matrix);
	printMatrix(&matrix);
}

void _checkMatrixPerVector(ScePspFMatrix4 *matrix, ScePspFVector4 *vmult, ScePspFVector4 *vresult) {
	asm volatile (
		"lv.q R700, 0x00+%1\n"
		"lv.q R701, 0x10+%1\n"
		"lv.q R702, 0x20+%1\n"
		"lv.q R703, 0x30+%1\n"
		
		"lv.q R600, 0x00+%2\n"
		
		"vtfm3.t R100, M700, R600\n"
		
		"sv.q    R100, 0x00+%0\n"
		: "+m" (*vresult) : "m" (*matrix), "m" (*vmult)
	);
}

void _checkHMatrixPerVector(ScePspFMatrix4 *matrix, ScePspFVector4 *vmult, ScePspFVector4 *vresult) {
	asm volatile (
		"lv.q R700, 0x00+%1\n"
		"lv.q R701, 0x10+%1\n"
		"lv.q R702, 0x20+%1\n"
		"lv.q R703, 0x30+%1\n"
		
		"lv.q R600, 0x00+%2\n"
		
		"vhtfm3.t R100, M700, R600\n"
		
		"sv.q    R100, 0x00+%0\n"
		: "+m" (*vresult) : "m" (*matrix), "m" (*vmult)
	);
}


void checkMatrixPerVector() {
	static ScePspFMatrix4 matrix = {
		{ 1.0f, 5.0f,  9.0f, -13.0f },
		{ 2.0f, -6.0f, 10.0f, 14.0f },
		{ 3.0f, 7.0f, 11.0f, 15.0f },
		{ 4.0f, 8.0f, -12.0f, 16.0f }
	};
	static ScePspFVector4 vmult = { -13.0f, -2111.0f, 33.0f, 40.0f};
	static ScePspFVector4 vout = { 0.0f, 0.0f, 0.0f, 0.0f };

	_checkMatrixPerVector(&matrix, &vmult, &vout);
	printVector(&vout);
	
	_checkHMatrixPerVector(&matrix, &vmult, &vout);
	printVector(&vout);
}

void _checkCrossProduct(ScePspFVector4 *vleft, ScePspFVector4 *vright, ScePspFVector4 *vresult) {
	asm volatile (
		"lv.q R500, 0x00+%1\n"
		"lv.q R600, 0x00+%2\n"
		
		"vcrsp.t R100, R500, R600\n"
		
		"sv.q    R100, 0x00+%0\n"
		: "+m" (*vresult) : "m" (*vleft), "m" (*vright)
	);
}

void _checkQuatProduct(ScePspFVector4 *vleft, ScePspFVector4 *vright, ScePspFVector4 *vresult) {
	asm volatile (
		"lv.q R500, 0x00+%1\n"
		"lv.q R600, 0x00+%2\n"
		
		"vqmul.q R100, R500, R600\n"
		
		"sv.q    R100, 0x00+%0\n"
		: "+m" (*vresult) : "m" (*vleft), "m" (*vright)
	);
}

void checkCrossProduct() {
	static __attribute__ ((aligned (16))) ScePspFVector4 vleft = { -1.0f, -2.0f, 3.0f, 4.0f};
	static __attribute__ ((aligned (16))) ScePspFVector4 vright = { -3.0f, 5.0f, 7.0f, -11.0f };
	static __attribute__ ((aligned (16))) ScePspFVector4 vout = { 0.0f, 0.0f, 0.0f, 0.0f };
	_checkCrossProduct(&vleft, &vright, &vout);
	printVector(&vout);
	_checkQuatProduct(&vleft, &vright, &vout);
	printVector(&vout);
}

void _checkHalfCrossProduct(ScePspFVector4 *vleft, ScePspFVector4 *vright, ScePspFVector4 *vresult) {
	asm volatile (
		"lv.q R500, 0x00+%1\n"
		"lv.q R600, 0x00+%2\n"
		
		"vcrs.t R100, R500, R600\n"
		
		"sv.q    R100, 0x00+%0\n"
		: "+m" (*vresult) : "m" (*vleft), "m" (*vright)
	);
}

void checkHalfCrossProduct() {
	static __attribute__ ((aligned (16))) ScePspFVector4 vleft = { -1.0f, -2.0f, 3.0f, 4.0f};
	static __attribute__ ((aligned (16))) ScePspFVector4 vright = { -3.0f, 5.0f, 7.0f, -11.0f };
	static __attribute__ ((aligned (16))) ScePspFVector4 vout = { 0.0f, 0.0f, 0.0f, 0.0f };
	_checkHalfCrossProduct(&vleft, &vright, &vout);
	printVector(&vout);
}

void _checkCompare(ScePspFVector4 *vleft, ScePspFVector4 *vright, ScePspFVector4 *vresult) {
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
	static __attribute__ ((aligned (16))) ScePspFVector4 vleft  = { 1.0f, -1.0f, -1.1f, 2.0f };
	static __attribute__ ((aligned (16))) ScePspFVector4 vright = { 1.0f,  1.0f, -1.1f, 2.1f };
	static __attribute__ ((aligned (16))) ScePspFVector4 vout = { 0.0f, 0.0f, 0.0f, 0.0f };
	_checkCompare(&vleft, &vright, &vout);
	printVector(&vout);
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

void _checkCompare2(float a, float b) {
 	static __attribute__ ((aligned (16))) ScePspFVector4 vleft;
	static __attribute__ ((aligned (16))) ScePspFVector4 vright;
 	ScePspFVector4 *vLeft = &vleft;
 	ScePspFVector4 *vRight = &vright;
  int i;
  for (i = 0; i < 4; i++) {
    memcpy(((float*)&vleft) + i, &a, 4);
    memcpy(((float*)&vright) + i, &b, 4);
  }
  int temp;
  unsigned int res[16];
  printf("=== Comparing %f, %f ===\n", a, b);
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
      printf("%f %f %s: %s\n", a, b, cmpNames[i], ((res[i]>>23)&1) ? "T" : "F");
    } else {
      printf("%f %f %s: %08x\n", a, b, cmpNames[i], res[i]);
    }
  }
}

void checkCompare2() {
  _checkCompare2(1.0f, 2.0f);
  _checkCompare2(2.0f, 1.0f);
  _checkCompare2(2.0f, 2.0f);
  _checkCompare2(INFINITY, INFINITY);
  _checkCompare2(-INFINITY, 100.0f);
  _checkCompare2(NAN, 0.0f);
  _checkCompare2(0.0f, NAN);
  _checkCompare2(NAN, NAN);
  _checkCompare2(NAN, -NAN);
  _checkCompare2(-NAN, -NAN);
  _checkCompare2(-0.0f, 0.0f);
}


int main(int argc, char *argv[]) {
	printf("Started\n");

	resetAllMatrices();
	
	printf("checkCompare:\n"); checkCompare();
	printf("checkCompare2:\n"); checkCompare2();
	printf("checkVF2I:\n"); checkVF2I();
	printf("checkVI2F:\n"); checkVI2F();
	printf("checkHalf:\n"); checkHalf();
	printf("checkCrossProduct:\n"); checkCrossProduct();
	printf("checkHalfCrossProduct:\n"); checkHalfCrossProduct();
	printf("checkButterfly1:\n"); checkVbfy1();
	printf("checkButterfly2:\n"); checkVbfy2();
	printf("checkMatrixPerVector:\n"); checkMatrixPerVector();
	printf("checkAggregated:\n"); checkAggregated();
	printf("checkSimpleLoad:\n"); checkSimpleLoad();
	printf("checkMisc:\n"); checkMisc();
	printf("checkTranspose:\n"); checkTranspose();
	printf("checkMultiplyFull:\n"); checkMultiplyFull();
	printf("checkVmin:\n"); checkVmin();
	printf("checkVmax:\n"); checkVmax();
	printf("checkVsge:\n"); checkVsge();
	printf("checkVslt:\n"); checkVslt();
	printf("checkVadd:\n"); checkVadd();
	printf("checkVsub:\n"); checkVsub();
	printf("checkVdiv:\n"); checkVdiv();
	printf("checkVmul:\n"); checkVmul();
	printf("checkVrcp:\n"); checkVrcp();
	printf("checkViim:\n"); checkViim();
  printf("checkVsgn:\n"); checkVsgn();
  printf("checkVsat0:\n"); checkVsat0();
	printf("checkLoadUnaligned:\n"); checkLoadUnaligned();
	printf("moveNormalRegister: "); moveNormalRegister();
	printf("checkVfim: "); checkVfim();
	printf("checkConstants:\n"); checkConstants();
	printf("checkVectorCopy: "); checkVectorCopy();
	printf("checkDot: "); checkDot();
	printf("checkScale: "); checkScale();
	printf("checkRotation: "); checkRotation();
	printf("checkMatrixIdentity:\n"); checkMatrixIdentity();
	printf("checkMultiply:\n"); checkMultiply();
	printf("checkVzero:\n"); checkVzero();
	printf("checkVone:\n"); checkVone();
	printf("checkGlRotate:\n"); checkGlRotate(argc, argv);
	printf("checkGum:\n"); checkGum();
	
	printf("checkMatrixScale:\n"); checkMatrixScale();
	
	//return 0;

	printf("Ended\n");
	
	/*
	while (1) {
		sceDisplayWaitVblankStart();
	}
	*/

	return 0;
}
