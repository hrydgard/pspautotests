#include <common.h>
#include "vfpu_common.h"
#include <string.h>

// vrot on every size and all 32 immediates, with the angle either in a separate register or in
// one of the destination lanes. The destination starts out as a marker, so lanes vrot leaves
// alone show up, and so does whether the cosine sees a sine already written over the angle.
// The assembler refuses both the odd immediates and the overlap, so vrot is emitted as a word:
// 0xF3A00000 | imm << 16 | vs << 8 | vd, plus 0x80 (.p), 0x8000 (.t) or 0x8080 (.q).

typedef struct { unsigned int x, y, z, w; } UVec4;

static ALIGN16 const UVec4 marker = { 0x7f800001, 0x3f800000, 0x40000000, 0x40400000 };

#define ROT_CASE(i, word, sreg) \
	case i: asm volatile (".set noreorder\n" \
		"lv.q C000, %1\n" \
		"mtv %2, " sreg "\n" \
		".word " #word " | (" #i " << 16)\n" \
		"nop\nnop\nnop\nnop\n" \
		"sv.q C000, %0\n" \
		".set reorder\n" : "=m" (*out) : "m" (marker), "r" (angle) : "memory"); break;

#define ROT_ALL(word, sreg) \
	ROT_CASE(0, word, sreg) ROT_CASE(1, word, sreg) ROT_CASE(2, word, sreg) ROT_CASE(3, word, sreg) \
	ROT_CASE(4, word, sreg) ROT_CASE(5, word, sreg) ROT_CASE(6, word, sreg) ROT_CASE(7, word, sreg) \
	ROT_CASE(8, word, sreg) ROT_CASE(9, word, sreg) ROT_CASE(10, word, sreg) ROT_CASE(11, word, sreg) \
	ROT_CASE(12, word, sreg) ROT_CASE(13, word, sreg) ROT_CASE(14, word, sreg) ROT_CASE(15, word, sreg) \
	ROT_CASE(16, word, sreg) ROT_CASE(17, word, sreg) ROT_CASE(18, word, sreg) ROT_CASE(19, word, sreg) \
	ROT_CASE(20, word, sreg) ROT_CASE(21, word, sreg) ROT_CASE(22, word, sreg) ROT_CASE(23, word, sreg) \
	ROT_CASE(24, word, sreg) ROT_CASE(25, word, sreg) ROT_CASE(26, word, sreg) ROT_CASE(27, word, sreg) \
	ROT_CASE(28, word, sreg) ROT_CASE(29, word, sreg) ROT_CASE(30, word, sreg) ROT_CASE(31, word, sreg)

#define ROT_FUNC(name, word, sreg) \
	static void __attribute__((noinline)) name(UVec4 *out, float angle, int imm) { \
		switch (imm) { ROT_ALL(word, sreg) } \
	}

// vd = C000. vs = S100 (4), S000 (0), S001 (32), S002 (64), S003 (96).
ROT_FUNC(rot_p_sep, 0xF3A00480, "S100")
ROT_FUNC(rot_p_l0,  0xF3A00080, "S000")
ROT_FUNC(rot_p_l1,  0xF3A02080, "S001")
ROT_FUNC(rot_t_sep, 0xF3A08400, "S100")
ROT_FUNC(rot_t_l0,  0xF3A08000, "S000")
ROT_FUNC(rot_t_l1,  0xF3A0A000, "S001")
ROT_FUNC(rot_t_l2,  0xF3A0C000, "S002")
ROT_FUNC(rot_q_sep, 0xF3A08480, "S100")
ROT_FUNC(rot_q_l0,  0xF3A08080, "S000")
ROT_FUNC(rot_q_l1,  0xF3A0A080, "S001")
ROT_FUNC(rot_q_l2,  0xF3A0C080, "S002")
ROT_FUNC(rot_q_l3,  0xF3A0E080, "S003")

typedef void (*RotFunc)(UVec4 *out, float angle, int imm);

static const struct {
	const char *name;
	RotFunc func;
} rots[] = {
	{ "vrot.p C000, S100", rot_p_sep },
	{ "vrot.p C000, S000", rot_p_l0 },
	{ "vrot.p C000, S001", rot_p_l1 },
	{ "vrot.t C000, S100", rot_t_sep },
	{ "vrot.t C000, S000", rot_t_l0 },
	{ "vrot.t C000, S001", rot_t_l1 },
	{ "vrot.t C000, S002", rot_t_l2 },
	{ "vrot.q C000, S100", rot_q_sep },
	{ "vrot.q C000, S000", rot_q_l0 },
	{ "vrot.q C000, S001", rot_q_l1 },
	{ "vrot.q C000, S002", rot_q_l2 },
	{ "vrot.q C000, S003", rot_q_l3 },
};

int main(int argc, char *argv[]) {
	// In units of pi/2. The second one makes sin and cos differ in sign.
	static const float angles[] = { 0.3f, -1.7f };
	printf("marker: %08x,%08x,%08x,%08x\n", marker.x, marker.y, marker.z, marker.w);
	for (int r = 0; r < ARRAY_SIZE(rots); r++) {
		for (int a = 0; a < ARRAY_SIZE(angles); a++) {
			printf("== %s, angle %g ==\n", rots[r].name, angles[a]);
			for (int imm = 0; imm < 32; imm++) {
				ALIGN16 UVec4 u;
				rots[r].func(&u, angles[a], imm);
				printf("imm %2d: %08x,%08x,%08x,%08x%s", imm, u.x, u.y, u.z, u.w, (imm % 2) ? "\n" : "   ");
			}
		}
	}
	return 0;
}
