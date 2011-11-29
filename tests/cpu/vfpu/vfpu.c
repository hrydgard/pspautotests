/*
FPU Test. Originally from jpcsp project:
http://code.google.com/p/jpcsp/source/browse/trunk/demos/src/fputest/main.c
Modified to perform automated tests.
*/

#include <common.h>

#include <pspkernel.h>
#include <stdio.h>
#include <string.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
//#include "../common/emits.h"

void __attribute__((noinline)) vcopy(ScePspFVector4 *v0, ScePspFVector4 *v1) {
	asm volatile (
		"lv.q   C100, %1\n"
		"sv.q   C100, %0\n"

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


ScePspFVector4 v0, v1, v2;
ScePspFVector4 matrix[4];

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
	ScePspFVector4 v[5];
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
		: "+m" (v)
	);
	char buf[1024];
	char temp[1024];
	buf[0] = 0;
	for (n = 0; n < 5; n++) {
		sprintf(temp, "%f,%f,%f,%f\n", v[n].x, v[n].y, v[n].z, v[n].w);
		strcat(buf, temp);
	}
	//printf("%s", buf);
	printf("%d\n", strcmp(buf,
		"inf,1.414214,0.707107,1.128379\n"
		"0.636620,0.318310,0.785398,1.570796\n"
		"3.141593,2.718282,1.442695,0.434294\n"
		"0.693147,2.302585,6.283185,0.523599\n"
		"0.301030,3.321928,0.866025,0.000000\n"
	));
	
	puts(buf);
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
	printf("%f, %f\n", v0.x, v0.y);
}

void moveNormalRegister() {
	float t = 5.0;
	//int t2 = *(int *)&t;
	ScePspFVector4 v;
	asm volatile(
		"mtv %1, S410\n"
		"mtv %1, S411\n"
		"mtv %1, S412\n"
		"mtv %1, S413\n"
		"sv.q   C410, 0x00+%0\n"
		: "+m" (v) : "t" (t)
	);
	printf("%f, %f, %f, %f\n", v.x, v.y, v.z, v.w);
}

void checkGlRotate() {
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

int main(int argc, char *argv[]) {
	printf("Started\n");

	printf("moveNormalRegister: "); moveNormalRegister();
	printf("checkVfim: "); checkVfim();
	printf("checkConstants:\n"); checkConstants();
	printf("checkVectorCopy: "); checkVectorCopy();
	printf("checkDot: "); checkDot();
	printf("checkScale: "); checkScale();
	printf("checkRotation: "); checkRotation();
	printf("checkMatrixIdentity:\n"); checkMatrixIdentity();

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(480, 272);
	glutCreateWindow(__FILE__);
	checkGlRotate();

	printf("Ended\n");

	return 0;
}