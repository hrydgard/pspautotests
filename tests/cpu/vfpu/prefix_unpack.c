#include <common.h>
#include "vfpu_common.h"
#include <string.h>

// The integer unpack ops (vuc2ifs, vc2i, vus2i, vs2i) with prefixes pending. They read bits, not
// floats, so what does a negate or a constant in the S prefix do to them, and what does an
// out-of-range swizzle do - the destination holds a marker so "zeroed" and "left alone" differ.
// Then the D prefix: saturation on a value that is really an integer, and the write mask.
//
// What the hardware does: a valid S prefix is applied to the raw bits (a negate flips the sign
// bit, a constant is the float's encoding), the D prefix saturates the results as floats, and the
// T prefix is ignored. An out-of-range swizzle doesn't zero the result like it does for the
// arithmetic ops (the last case below checks that it still does there) - it replays a value the
// prefix unit produced earlier, usually the last prefixed S read, even from another op. The
// update rule isn't clean (see the "after unprefixed vmov" case), so this is recorded rather than
// understood, and an emulator substituting zero is being reasonable.

typedef struct { unsigned int x, y, z, w; } UVec4;

static ALIGN16 const UVec4 input = { 0x3fc00000, 0x8040c0ff, 0x00010203, 0x7f80ff01 };
static ALIGN16 const UVec4 marker = { 0x42280000, 0x422c0000, 0x42300000, 0x42340000 };

#define UNPACK(name, instr, sz, sreg, pfx) \
	static void __attribute__((noinline)) name(UVec4 *out) { \
		asm volatile ( \
			"lv.q   C100, %1\n" \
			"lv.q   C000, %2\n" \
			pfx \
			instr "." sz " C000, " sreg "100\n" \
			"sv.q   C000, %0\n" \
			: "+m" (*out) : "m" (input), "m" (marker) : "memory" \
		); \
	}

UNPACK(uc_plain,  "vuc2ifs", "s", "S", "")
UNPACK(uc_sx,     "vuc2ifs", "s", "S", "vpfxs x, y, z, w\n")
UNPACK(uc_snegx,  "vuc2ifs", "s", "S", "vpfxs -x, y, z, w\n")
UNPACK(uc_sabsx,  "vuc2ifs", "s", "S", "vpfxs |x|, y, z, w\n")
UNPACK(uc_sc1,    "vuc2ifs", "s", "S", "vpfxs 1, y, z, w\n")
UNPACK(uc_sc2,    "vuc2ifs", "s", "S", "vpfxs 2, y, z, w\n")
UNPACK(uc_sy,     "vuc2ifs", "s", "S", "vpfxs y, y, z, w\n")
UNPACK(uc_snegy,  "vuc2ifs", "s", "S", "vpfxs -y, -x, -w, -z\n")
UNPACK(uc_sw,     "vuc2ifs", "s", "S", "vpfxs w, y, z, w\n")
UNPACK(uc_t,      "vuc2ifs", "s", "S", "vpfxt 1, 1/2, 3, 0\n")
UNPACK(uc_dsat01, "vuc2ifs", "s", "S", "vpfxd 0:1, 0:1, 0:1, 0:1\n")
UNPACK(uc_dsat11, "vuc2ifs", "s", "S", "vpfxd -1:1, -1:1, -1:1, -1:1\n")
UNPACK(uc_dmask,  "vuc2ifs", "s", "S", "vpfxd m, , m, \n")
UNPACK(uc_dsatx,  "vuc2ifs", "s", "S", "vpfxd 0:1,,,\n")
UNPACK(uc_all,    "vuc2ifs", "s", "S", "vpfxs -y, -x, -w, -z\nvpfxt 1, 1/2, 3, 0\nvpfxd 0:1, 0:1, 0:1, 0:1\n")

UNPACK(c_plain,   "vc2i", "s", "S", "")
UNPACK(c_snegx,   "vc2i", "s", "S", "vpfxs -x, y, z, w\n")
UNPACK(c_sy,      "vc2i", "s", "S", "vpfxs y, y, z, w\n")
UNPACK(c_dsat01,  "vc2i", "s", "S", "vpfxd 0:1, 0:1, 0:1, 0:1\n")

UNPACK(us_plain,  "vus2i", "p", "C", "")
UNPACK(us_snegx,  "vus2i", "p", "C", "vpfxs -x, y, z, w\n")
UNPACK(us_sswap,  "vus2i", "p", "C", "vpfxs y, x, z, w\n")
UNPACK(us_sz,     "vus2i", "p", "C", "vpfxs z, w, z, w\n")
UNPACK(us_sxz,    "vus2i", "p", "C", "vpfxs x, z, z, w\n")
UNPACK(us_dsat01, "vus2i", "p", "C", "vpfxd 0:1, 0:1, 0:1, 0:1\n")

UNPACK(s_plain,   "vs2i", "p", "C", "")
UNPACK(s_snegx,   "vs2i", "p", "C", "vpfxs -x, y, z, w\n")
UNPACK(s_sz,      "vs2i", "p", "C", "vpfxs z, w, z, w\n")
UNPACK(s_dsat01,  "vs2i", "p", "C", "vpfxd 0:1, 0:1, 0:1, 0:1\n")

// An invalid swizzle looks like it replays whatever the prefix unit last produced. Feed it a known
// value through a different op first, prefixed and unprefixed, and see what comes back.
static void __attribute__((noinline)) latch_prefixed_vmov(UVec4 *out) {
	asm volatile (
		"lv.q   C100, %1\n"
		"lv.q   C000, %2\n"
		"vpfxs  y, y, z, w\n"
		"vmov.q C200, C100\n"        // prefixed read of input.y into the prefix unit
		"vpfxs  y, y, z, w\n"
		"vuc2ifs.s C000, S100\n"     // invalid: replays?
		"sv.q   C000, %0\n"
		: "+m" (*out) : "m" (input), "m" (marker) : "memory"
	);
}
static void __attribute__((noinline)) latch_prefixed_vadd(UVec4 *out) {
	asm volatile (
		"lv.q   C100, %1\n"
		"lv.q   C000, %2\n"
		"vpfxt  z, z, z, w\n"
		"vadd.s S200, S100, S100\n"  // a prefixed T read of input.z
		"vpfxs  y, y, z, w\n"
		"vuc2ifs.s C000, S100\n"
		"sv.q   C000, %0\n"
		: "+m" (*out) : "m" (input), "m" (marker) : "memory"
	);
}
static void __attribute__((noinline)) latch_unprefixed(UVec4 *out) {
	asm volatile (
		"lv.q   C100, %1\n"
		"lv.q   C000, %2\n"
		"vmov.s S200, S130\n"        // unprefixed read of input.w
		"vpfxs  y, y, z, w\n"
		"vuc2ifs.s C000, S100\n"
		"sv.q   C000, %0\n"
		: "+m" (*out) : "m" (input), "m" (marker) : "memory"
	);
}
static void __attribute__((noinline)) latch_vadd_invalid(UVec4 *out) {
	// And does a plain vadd with an invalid swizzle replay too, or write zero as prefix_ctrl saw?
	asm volatile (
		"lv.q   C100, %1\n"
		"lv.q   C000, %2\n"
		"vpfxs  x, y, z, w\n"
		"vmov.q C200, C100\n"        // prefixed read of all of input
		"vpfxs  w, w, w, w\n"
		"vadd.s S000, S100, S100\n"  // invalid on a single
		"sv.q   C000, %0\n"
		: "+m" (*out) : "m" (input), "m" (marker) : "memory"
	);
}

typedef void (*Func)(UVec4 *);
static const struct { const char *name; Func func; } tests[] = {
	{ "vuc2ifs.s", uc_plain }, { "vuc2ifs.s s[x]", uc_sx }, { "vuc2ifs.s s[-x]", uc_snegx },
	{ "vuc2ifs.s s[|x|]", uc_sabsx }, { "vuc2ifs.s s[1]", uc_sc1 }, { "vuc2ifs.s s[2]", uc_sc2 },
	{ "vuc2ifs.s s[y]", uc_sy }, { "vuc2ifs.s s[-y,-x,-w,-z]", uc_snegy }, { "vuc2ifs.s s[w]", uc_sw },
	{ "vuc2ifs.s t[1,1/2,3,0]", uc_t },
	{ "vuc2ifs.s d[0:1 x4]", uc_dsat01 }, { "vuc2ifs.s d[-1:1 x4]", uc_dsat11 },
	{ "vuc2ifs.s d[m,,m,]", uc_dmask }, { "vuc2ifs.s d[0:1,,,]", uc_dsatx },
	{ "vuc2ifs.s s[-y..] t[..] d[0:1]", uc_all },
	{ "vc2i.s", c_plain }, { "vc2i.s s[-x]", c_snegx }, { "vc2i.s s[y]", c_sy }, { "vc2i.s d[0:1 x4]", c_dsat01 },
	{ "vus2i.p", us_plain }, { "vus2i.p s[-x,y]", us_snegx }, { "vus2i.p s[y,x]", us_sswap },
	{ "vus2i.p s[z,w]", us_sz }, { "vus2i.p s[x,z]", us_sxz }, { "vus2i.p d[0:1 x4]", us_dsat01 },
	{ "vs2i.p", s_plain }, { "vs2i.p s[-x,y]", s_snegx }, { "vs2i.p s[z,w]", s_sz }, { "vs2i.p d[0:1 x4]", s_dsat01 },
	{ "after prefixed vmov of .y", latch_prefixed_vmov },
	{ "after prefixed vadd T of .z", latch_prefixed_vadd },
	{ "after unprefixed vmov of .w", latch_unprefixed },
	{ "vadd.s invalid after vmov", latch_vadd_invalid },
};

int main(int argc, char *argv[]) {
	ALIGN16 UVec4 out;
	printf("input %08x,%08x,%08x,%08x  marker %08x,%08x,%08x,%08x\n", input.x, input.y, input.z, input.w, marker.x, marker.y, marker.z, marker.w);
	for (int i = 0; i < ARRAY_SIZE(tests); i++) {
		tests[i].func(&out);
		printf("%-32s %08x,%08x,%08x,%08x\n", tests[i].name, out.x, out.y, out.z, out.w);
	}
	return 0;
}
