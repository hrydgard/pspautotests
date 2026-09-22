#include <common.h>
#include "vfpu_common.h"
#include <string.h>
#include <math.h>

// The special values (NaN, the infinities, the two zeros, denormals) through the VFPU ops whose
// handling of them nothing else pins down: every vcmp condition, the sorts, the half-float
// conversions, vrot's whole immediate range, and a few unary ops.

typedef struct { unsigned int x, y, z, w; } UVec4;

// s lanes: NaN, -NaN, inf, -inf / 0, -0, denorm, -denorm / 1, -1, 1e30, -1e-30 ... as three quads,
// each compared against t = { 1, NaN, 0, -inf } and against itself.
static ALIGN16 const ScePspFVector4 sA = { NAN, -NAN, INFINITY, -INFINITY };
static ALIGN16 const ScePspFVector4 sB = { 0.0f, -0.0f, 1e-40f, -1e-40f };
static ALIGN16 const ScePspFVector4 sC = { 1.0f, -1.0f, 1e30f, -1e-30f };
static ALIGN16 const ScePspFVector4 tA = { 1.0f, NAN, 0.0f, -INFINITY };
static ALIGN16 const ScePspFVector4 tB = { NAN, NAN, NAN, NAN };
static ALIGN16 const ScePspFVector4 tC = { 1e-40f, 0.0f, -0.0f, -1e-40f };

#define VCMP(cond) \
	static unsigned int __attribute__((noinline)) vcmp_##cond(const ScePspFVector4 *s, const ScePspFVector4 *t) { \
		unsigned int cc; \
		asm volatile ( \
			".set noreorder\n" \
			"lv.q   C100, %1\n" \
			"lv.q   C110, %2\n" \
			"vcmp.q " #cond ", C100, C110\n" \
			"nop\nnop\nnop\nnop\n" \
			"mfvc   %0, $131\n" \
			".set reorder\n" \
			: "=r" (cc) : "m" (*s), "m" (*t) : "memory" \
		); \
		return cc & 0x3F; \
	}
VCMP(FL) VCMP(EQ) VCMP(LT) VCMP(LE) VCMP(TR) VCMP(NE) VCMP(GE) VCMP(GT)
VCMP(EZ) VCMP(EN) VCMP(EI) VCMP(ES) VCMP(NZ) VCMP(NN) VCMP(NI) VCMP(NS)

typedef unsigned int (*CmpFunc)(const ScePspFVector4 *, const ScePspFVector4 *);
static const struct { const char *name; CmpFunc func; } conds[] = {
	{ "FL", vcmp_FL }, { "EQ", vcmp_EQ }, { "LT", vcmp_LT }, { "LE", vcmp_LE }, { "TR", vcmp_TR }, { "NE", vcmp_NE }, { "GE", vcmp_GE }, { "GT", vcmp_GT },
	{ "EZ", vcmp_EZ }, { "EN", vcmp_EN }, { "EI", vcmp_EI }, { "ES", vcmp_ES }, { "NZ", vcmp_NZ }, { "NN", vcmp_NN }, { "NI", vcmp_NI }, { "NS", vcmp_NS },
};

#define UNARY(name, instr) \
	static void __attribute__((noinline)) name(ScePspFVector4 *out, const ScePspFVector4 *a) { \
		asm volatile ("lv.q C100, %1\n" instr "\nsv.q C000, %0\n" : "+m" (*out) : "m" (*a) : "memory"); \
	}
UNARY(vsrt1_q, "vsrt1.q C000, C100")
UNARY(vsrt2_q, "vsrt2.q C000, C100")
UNARY(vsrt3_q, "vsrt3.q C000, C100")
UNARY(vsrt4_q, "vsrt4.q C000, C100")
UNARY(vsgn_q,  "vsgn.q C000, C100")
UNARY(vocp_q,  "vocp.q C000, C100")
UNARY(vbfy1_q, "vbfy1.q C000, C100")
UNARY(vbfy2_q, "vbfy2.q C000, C100")
UNARY(vavg_q,  "vmov.q C000, C000[0,0,0,0]\nvavg.q S000, C100")
UNARY(vfad_q,  "vmov.q C000, C000[0,0,0,0]\nvfad.q S000, C100")
UNARY(vf2h_q,  "vmov.q C000, C000[0,0,0,0]\nvf2h.q C000, C100")
UNARY(vh2f_p,  "vh2f.p C000, C100")
UNARY(vh2f_p_hi, "vh2f.p C000, C102")  // rows 2-3 of the loaded column
UNARY(vsocp_p, "vsocp.p C000, C100")

static void showv(const char *label, const ScePspFVector4 *v) {
	UVec4 u; memcpy(&u, v, 16);
	printf("%-28s %08x,%08x,%08x,%08x\n", label, u.x, u.y, u.z, u.w);
}
static void showin(const char *label, const ScePspFVector4 *v) {
	UVec4 u; memcpy(&u, v, 16);
	printf("-- %s: %08x,%08x,%08x,%08x --\n", label, u.x, u.y, u.z, u.w);
}

// vrot: every immediate, for a few angles. The imm5 encodes where sin and cos go and a negation;
// the assembler only accepts the sensible ones by name, so all 32 are emitted as words.
// vrot.q C000, S100 assembles to 0xF3A08480 | (imm << 16).
static void __attribute__((noinline)) vrot_q(ScePspFVector4 *out, float angle, int imm) {
	ALIGN16 ScePspFVector4 tmp;
	switch (imm) {
	case 0: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3A08480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 1: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3A18480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 2: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3A28480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 3: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3A38480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 4: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3A48480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 5: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3A58480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 6: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3A68480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 7: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3A78480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 8: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3A88480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 9: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3A98480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 10: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3AA8480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 11: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3AB8480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 12: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3AC8480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 13: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3AD8480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 14: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3AE8480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 15: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3AF8480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 16: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3B08480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 17: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3B18480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 18: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3B28480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 19: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3B38480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 20: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3B48480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 21: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3B58480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 22: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3B68480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 23: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3B78480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 24: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3B88480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 25: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3B98480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 26: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3BA8480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 27: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3BB8480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 28: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3BC8480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 29: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3BD8480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 30: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3BE8480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	case 31: asm volatile (".set noreorder\nmtv %1, S100\nvmov.q C000, C000[0,0,0,0]\n.word 0xF3BF8480\nnop\nnop\nnop\nnop\nsv.q C000, %0\n.set reorder\n" : "+m" (tmp) : "r" (angle) : "memory"); break;
	}
	*out = tmp;
}

int main(int argc, char *argv[]) {
	ALIGN16 ScePspFVector4 out;
	asm volatile ("vnop\nvnop\n" : : : "memory");

	printf("== vcmp, CC after (bits 0-3 lanes, 4 any, 5 all) ==\n");
	const ScePspFVector4 *ss[] = { &sA, &sB, &sC };
	const ScePspFVector4 *ts[] = { &tA, &tB, &tC };
	const char *sn[] = { "nan,-nan,inf,-inf", "0,-0,den,-den", "1,-1,1e30,-1e-30" };
	const char *tn[] = { "1,nan,0,-inf", "nan x4", "den,0,-0,-den" };
	for (int si = 0; si < 3; si++) {
		for (int ti = 0; ti < 3; ti++) {
			printf("s=%-18s t=%-14s", sn[si], tn[ti]);
			for (int c = 0; c < ARRAY_SIZE(conds); c++) {
				printf(" %s=%02x", conds[c].name, conds[c].func(ss[si], ts[ti]));
			}
			printf("\n");
		}
		printf("s=%-18s t=self         ", sn[si]);
		for (int c = 0; c < ARRAY_SIZE(conds); c++) {
			printf(" %s=%02x", conds[c].name, conds[c].func(ss[si], ss[si]));
		}
		printf("\n");
	}

	printf("== sorts, butterflies, sign, one's complement, average, sum ==\n");
	static ALIGN16 const ScePspFVector4 srt[] = {
		{ NAN, 1.0f, -1.0f, -NAN }, { -0.0f, 0.0f, -0.0f, 0.0f }, { INFINITY, -INFINITY, 1e-40f, -1e-40f },
		{ 3.0f, NAN, 1.0f, 2.0f }, { -1e-40f, 1e-40f, -0.0f, 0.0f }, { 2.0f, 2.0f, -2.0f, -2.0f },
	};
	for (int i = 0; i < ARRAY_SIZE(srt); i++) {
		showin("input", &srt[i]);
		vsrt1_q(&out, &srt[i]); showv("vsrt1", &out);
		vsrt2_q(&out, &srt[i]); showv("vsrt2", &out);
		vsrt3_q(&out, &srt[i]); showv("vsrt3", &out);
		vsrt4_q(&out, &srt[i]); showv("vsrt4", &out);
		vbfy1_q(&out, &srt[i]); showv("vbfy1", &out);
		vbfy2_q(&out, &srt[i]); showv("vbfy2", &out);
		vsgn_q(&out, &srt[i]);  showv("vsgn", &out);
		vocp_q(&out, &srt[i]);  showv("vocp", &out);
		vavg_q(&out, &srt[i]);  showv("vavg (lane x)", &out);
		vfad_q(&out, &srt[i]);  showv("vfad (lane x)", &out);
		vsocp_p(&out, &srt[i]); showv("vsocp.p", &out);
	}

	printf("== vf2h: float -> half ==\n");
	static ALIGN16 const ScePspFVector4 f2h[] = {
		{ NAN, -NAN, INFINITY, -INFINITY },
		{ 0.0f, -0.0f, 1e-40f, -1e-40f },
		{ 65504.0f, 65520.0f, 65536.0f, 1e30f },      // max half, halfway to overflow, over
		{ 6.1035156e-05f, 6.0975552e-05f, 5.9604645e-08f, 2.9802322e-08f },  // smallest normal half, a subnormal, smallest subnormal, half of it
		{ 1.0009766f, 1.0004883f, 1.0014648f, -1.0004883f },  // 1 + 1ulp, 1 + half ulp (tie), 1 + 1.5 ulp, tie negative
		{ 2049.0f, 2050.0f, 2051.0f, 4097.0f },     // ties and rounding above the half's integer range
	};
	// NaN payloads: 1, the quiet bit, one that lands in the half's top mantissa bit, and one in both.
	static ALIGN16 const UVec4 f2hNaN[] = { { 0x7f800001, 0x7fc00000, 0x7f802000, 0xffbff001 } };
	printf("-- input: %08x,%08x,%08x,%08x --\n", f2hNaN[0].x, f2hNaN[0].y, f2hNaN[0].z, f2hNaN[0].w);
	vf2h_q(&out, (const ScePspFVector4 *)&f2hNaN[0]); showv("vf2h", &out);
	// Denormal inputs with both signs, and the smallest value above the flush threshold.
	static ALIGN16 const UVec4 f2hDen[] = { { 0x807fffff, 0x387fffff, 0xb87fffff, 0x38800001 } };
	printf("-- input: %08x,%08x,%08x,%08x --\n", f2hDen[0].x, f2hDen[0].y, f2hDen[0].z, f2hDen[0].w);
	vf2h_q(&out, (const ScePspFVector4 *)&f2hDen[0]); showv("vf2h", &out);
	for (int i = 0; i < ARRAY_SIZE(f2h); i++) {
		showin("input", &f2h[i]);
		vf2h_q(&out, &f2h[i]); showv("vf2h", &out);
	}

	printf("== vrot.q, all 32 immediates ==\n");
	static const float angles[] = { 0.0f, 0.5f, 1.0f, 1.5f, 2.0f, -0.5f, 0.25f, 3.75f };
	for (int a = 0; a < ARRAY_SIZE(angles); a++) {
		printf("-- angle %g (in units of pi/2) --\n", angles[a]);
		for (int imm = 0; imm < 32; imm++) {
			UVec4 u;
			vrot_q(&out, angles[a], imm);
			memcpy(&u, &out, 16);
			printf("imm %2d: %08x,%08x,%08x,%08x%s", imm, u.x, u.y, u.z, u.w, (imm % 2) ? "\n" : "   ");
		}
	}
	printf("-- special angles, imm 0 ([c,s,s,s]) --\n");
	static const float sangles[] = { NAN, INFINITY, -INFINITY, 1e-40f, 1e30f, 4.0f, -4.0f, 1e6f };
	for (int a = 0; a < ARRAY_SIZE(sangles); a++) {
		UVec4 u, in; memcpy(&in, &sangles[a], 4);
		vrot_q(&out, sangles[a], 0); memcpy(&u, &out, 16);
		printf("angle %08x: %08x,%08x,%08x,%08x\n", in.x, u.x, u.y, u.z, u.w);
	}

	printf("== vh2f: half -> float ==\n");
	static ALIGN16 const UVec4 h2f[] = {
		{ 0x7c007e00, 0xfc00fe00, 0x00008000, 0x00010200 },   // inf, nan / -inf, -nan / 0, -0 / smallest subnormal, a subnormal
		{ 0x03ff0400, 0x7bff3c00, 0xbc00c000, 0x7c01fc01 },   // largest subnormal, smallest normal / max, 1 / -1, -2 / nan with payload 1
		{ 0x00010200, 0x80018200, 0x83ff8000, 0x7c01fe01 },   // subnormals of both signs, -0, nans with payload 1
		{ 0x7fff0001, 0x7e017c01, 0x8001bfff, 0x84000401 },   // all-ones nan, ...
	};
	for (int i = 0; i < ARRAY_SIZE(h2f); i++) {
		printf("-- input: %08x,%08x,%08x,%08x --\n", h2f[i].x, h2f[i].y, h2f[i].z, h2f[i].w);
		vh2f_p(&out, (const ScePspFVector4 *)&h2f[i]); showv("vh2f lo pair", &out);
		vh2f_p_hi(&out, (const ScePspFVector4 *)&h2f[i]); showv("vh2f hi pair", &out);
	}
	return 0;
}
