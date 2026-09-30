#include <common.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <string.h>

// How the GE computes depth for transformed vertices. Clip Z is kept as a 24-bit float (the low 8
// mantissa bits dropped). z/w is z times the GE's reciprocal of w (a linear interpolation over 128
// segments, not quite 1/w), truncated to float24. The viewport's z * scale is a float24 too, and
// adding the center truncates both terms to the precision of the larger one (a 16-bit significand).
// The sum is floored.
//
// The same kind of add (without guard bits) puts a matrix's translation onto the product. So a
// center of -65535 with a product above 65536 only leaves even depths, and a large Z translation
// leaves depth in steps of a float24 ulp of the clip Z. Test Drive Unlimited's map depends on this:
// its vertices land just around its minimum depth of 16 (#12786).

static unsigned int __attribute__((aligned(16))) list[65536];

static u8 *const fbp0 = 0;
static u8 *const zbp0 = (u8 *)(512 * 272 * 4);
// Depth drawn with a 32-bit color buffer, deswizzled.
static volatile u16 *const depthMirror = (u16 *)(0x44600000 + 512 * 272 * 4);

struct Vert {
	float x, y, z;
};

static Vert __attribute__((aligned(16))) verts[64];

static const ScePspFMatrix4 identity = {
	{ 1, 0, 0, 0 },
	{ 0, 1, 0, 0 },
	{ 0, 0, 1, 0 },
	{ 0, 0, 0, 1 },
};

static void begin(const ScePspFMatrix4 &proj, float zscale, float zcenter, int minz) {
	sceGuStart(GU_DIRECT, list);
	sceGuClearDepth(0x1234);
	sceGuClear(GU_DEPTH_BUFFER_BIT);
	sceGuSetMatrix(GU_PROJECTION, &proj);
	sceGuSendCommandf(0x44, zscale);
	sceGuSendCommandf(0x47, zcenter);
	sceGuSendCommandi(0xD6, minz);
	sceGuSendCommandi(0xD7, 65535);
}

// Draws point i at pixel (i, 0), with clip coordinates (x * w / 240 ..., z) as given.
static void draw(int n) {
	sceKernelDcacheWritebackRange(verts, sizeof(verts));
	sceGuDrawArray(GU_POINTS, GU_VERTEX_32BITF | GU_TRANSFORM_3D, n, NULL, verts);
	sceGuFinish();
	sceGuSync(0, 0);
	// Waiting for the GE reschedules on hardware but not always in an emulator; make it certain.
	sceKernelDelayThread(100);
}

static void place(int i, float z, float w) {
	// Pixel (4 * i + 2, 2), whatever w is.
	verts[i].x = ((4 * i + 2 + 0.5f) / 240.0f - 1.0f) * w;
	verts[i].y = (1.0f - (2 + 0.5f) / 136.0f) * w;
	verts[i].z = z;
}

static u16 depthAt(int i) {
	return depthMirror[2 * 512 + 4 * i + 2];
}

static void testViewportAdd() {
	checkpointNext("Viewport add: z * 131071 - 65535, z in [0.5, 1)");
	static const int ks[] = { 1, 2, 3, 100, 1001, 16381, 20000, 32760 };
	const int n = sizeof(ks) / sizeof(ks[0]);
	begin(identity, 131071.0f, -65535.0f, 0);
	for (int i = 0; i < n; ++i) {
		place(i, 0.5f + ks[i] / 65536.0f, 1.0f);
	}
	draw(n);
	for (int i = 0; i < n; ++i) {
		checkpoint("  z = 0.5 + %d/65536: %d", ks[i], depthAt(i));
	}
}

static void testClipZ(float w) {
	char title[128];
	snprintf(title, sizeof(title), "Clip Z: 0.75 from the projection, w = %g", w);
	checkpointNext(title);
	static const int ks[] = { 500, 1100, 1600, 2100, 3000, 5000, 9000, 40000 };
	const int n = sizeof(ks) / sizeof(ks[0]);
	ScePspFMatrix4 proj = identity;
	proj.x.x = w; proj.y.y = w; proj.z.z = w; proj.w.w = w;
	proj.w.z = w * 0.75f;
	const float S = 65535.0f * 1024.0f;
	begin(proj, S, -0.75f * S, 0);
	for (int i = 0; i < n; ++i) {
		// place() scales x and y by w itself; the projection does it here.
		verts[i].x = (4 * i + 2 + 0.5f) / 240.0f - 1.0f;
		verts[i].y = 1.0f - (2 + 0.5f) / 136.0f;
		verts[i].z = ks[i] / 67108864.0f;
	}
	draw(n);
	for (int i = 0; i < n; ++i) {
		checkpoint("  z offset %d/2^26: %d", ks[i], depthAt(i));
	}
}

extern "C" int main(int argc, char *argv[]) {
	sceGuInit();
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888, fbp0, 512);
	sceGuDispBuffer(480, 272, fbp0, 512);
	sceGuDepthBuffer(zbp0, 512);
	sceGuOffset(2048 - (480 / 2), 2048 - (272 / 2));
	sceGuViewport(2048, 2048, 480, 272);
	sceGuScissor(0, 0, 480, 272);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuDisable(GU_CLIP_PLANES);
	sceGuDisable(GU_TEXTURE_2D);
	sceGuDisable(GU_LIGHTING);
	sceGuEnable(GU_DEPTH_TEST);
	sceGuDepthFunc(GU_ALWAYS);
	sceGuDepthMask(GU_FALSE);
	sceGuSetMatrix(GU_MODEL, &identity);
	sceGuSetMatrix(GU_VIEW, &identity);
	sceGuSetMatrix(GU_PROJECTION, &identity);
	sceGuFinish();
	sceGuSync(0, 0);

	testViewportAdd();
	testClipZ(1.0f);
	testClipZ(3.0f);

	sceGuTerm();
	return 0;
}
