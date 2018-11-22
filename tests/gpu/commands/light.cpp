#include <cstring>
#include <string>
#include <vector>
#include "shared.h"

extern "C" int HAS_DISPLAY;

struct BoxInfo {
	int x;
	int y;
	std::string title;
};

static u32 __attribute__((aligned(16))) rowBuffer[512];
static std::vector<BoxInfo> boxInfo;
static int boxNextX = 2;
static int boxNextY = 2;

static float boxNormZ = 1.0f;

static void drawBox(const char *title) {
	int x = boxNextX;
	int y = boxNextY;
	u32 col = 0xFFFFFFFF;

	Vertices v(GU_COLOR_8888 | GU_NORMAL_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_3D);
	v.CNP(col, 0, 0, boxNormZ, x, y, 0);
	v.CNP(col, 0, 0, boxNormZ, x + 2, y, 0);
	v.CNP(col, 0, 0, boxNormZ, x + 2, y + 2, 0);
	v.CNP(col, 0, 0, boxNormZ, x, y + 2, 0);

	void *p = sceGuGetMemory(v.Size());
	memcpy(p, v.Ptr(), v.Size());
	sceKernelDcacheWritebackInvalidateRange(p, v.Size());
	sceGuDrawArray(GU_TRIANGLE_FAN, v.VType(), 4, NULL, p);

	BoxInfo info = { x, y, title };
	boxInfo.push_back(info);

	boxNextX += 2;
	if (boxNextX >= 480) {
		boxNextX = 0;
		boxNextY += 2;
	}
}

static void calcNextLightPos(ScePspFVector3 *pos) {
	// For non-directional lights, this gives us a direction of approximately 0.
	pos->x = norm32x(boxNextX + 1);
	pos->y = -norm32x(boxNextY + 1);
	pos->z = 0.0f;
}

static void logBoxes() {
	endFrame();
	for (size_t i = 0; i < boxInfo.size(); ++i) {
		const BoxInfo *box = &boxInfo[i];
		sceKernelDcacheWritebackInvalidateRange(rowBuffer, 512 * sizeof(u32));
		sceDmacMemcpy(rowBuffer, (uintptr_t)sceGeEdramGetAddr() + fbp0 + box->y * 512 * 4, 512 * sizeof(u32));
		sceKernelDcacheWritebackInvalidateRange(rowBuffer, 512 * sizeof(u32));

		checkpoint("%s: %06x", box->title.c_str(), rowBuffer[box->x]);
	}
	boxInfo.clear();
	startFrame();
}

static void resetLightState() {
	// Simple, no distance multiplier by default.
	sceGuLightAtt(0, 1.0f, 0.0f, 0.0f);
	sceGuLightMode(GU_SINGLE_COLOR);
	sceGuLightColor(0, GU_AMBIENT, 0xFF0000);
	sceGuLightColor(0, GU_DIFFUSE, 0x00FF00);
	sceGuLightColor(0, GU_SPECULAR, 0x0000FF);

	ScePspFVector3 ldir0 = { 0.0f, 0.0f, 1.0f };
	sceGuLightSpot(0, &ldir0, 1.0f, 0.0f);
	sceGuSpecular(1.0f);

	sceGuModelColor(0x000000, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF);
	sceGuColorMaterial(0);

	boxNormZ = 1.0f;
}

static void drawLight(const char *title, const char *subtitle, int l, int type, int components, ScePspFVector3 *pos, ScePspFVector3 *add) {
	ScePspFVector3 realpos;
	if (pos != NULL) {
		memcpy(&realpos, pos, sizeof(realpos));
	} else {
		calcNextLightPos(&realpos);
	}

	realpos.x += add->x;
	realpos.y += add->y;
	realpos.z += add->z;

	sceGuLight(l, type, components, &realpos);
	char full[1024] = {};
	sprintf(full, "%s - %s", title, subtitle);
	drawBox(full);
}

enum LightKinds {
	LKIND_TYPE_DIREC = 0x01,
	LKIND_TYPE_POINT = 0x02,
	LKIND_TYPE_SPOT = 0x04,
	LKIND_TYPE_ALL = LKIND_TYPE_DIREC | LKIND_TYPE_POINT | LKIND_TYPE_SPOT,

	LKIND_MODE_ONLYDIFFUSE = 0x10,
	LKIND_MODE_BOTH = 0x20,
	LKIND_MODE_POWDIFFUSE = 0x40,
	LKIND_MODE_ALL = LKIND_MODE_ONLYDIFFUSE | LKIND_MODE_BOTH | LKIND_MODE_POWDIFFUSE,

	LKIND_ALL = LKIND_TYPE_ALL | LKIND_MODE_ALL,
};

static bool matchKind(int kinds, int match) {
	return (kinds & match) == match;
}

static void testLightTypes(const char *title, ScePspFVector3 *add, int kinds) {
	ScePspFVector3 pos = { 0.0f, 0.0f, 0.0f };
	if (matchKind(kinds, LKIND_TYPE_DIREC | LKIND_MODE_ONLYDIFFUSE)) {
		drawLight(title, "Direc A + D", 0, GU_DIRECTIONAL, GU_AMBIENT_AND_DIFFUSE, &pos, add);
	}
	if (matchKind(kinds, LKIND_TYPE_DIREC | LKIND_MODE_BOTH)) {
		drawLight(title, "Direc A + D + pow(S)", 0, GU_DIRECTIONAL, GU_DIFFUSE_AND_SPECULAR, &pos, add);
	}
	if (matchKind(kinds, LKIND_TYPE_DIREC | LKIND_MODE_POWDIFFUSE)) {
		drawLight(title, "Direc A + pow(D)", 0, GU_DIRECTIONAL, GU_UNKNOWN_LIGHT_COMPONENT, &pos, add);
	}
	if (matchKind(kinds, LKIND_TYPE_POINT | LKIND_MODE_ONLYDIFFUSE)) {
		drawLight(title, "Point A + D", 0, GU_POINTLIGHT, GU_AMBIENT_AND_DIFFUSE, NULL, add);
	}
	if (matchKind(kinds, LKIND_TYPE_POINT | LKIND_MODE_BOTH)) {
		drawLight(title, "Point A + D + pow(S)", 0, GU_POINTLIGHT, GU_DIFFUSE_AND_SPECULAR, NULL, add);
	}
	if (matchKind(kinds, LKIND_TYPE_POINT | LKIND_MODE_POWDIFFUSE)) {
		drawLight(title, "Point A + pow(D)", 0, GU_POINTLIGHT, GU_UNKNOWN_LIGHT_COMPONENT, NULL, add);
	}
	if (matchKind(kinds, LKIND_TYPE_SPOT | LKIND_MODE_ONLYDIFFUSE)) {
		drawLight(title, "Spot A + D", 0, GU_SPOTLIGHT, GU_AMBIENT_AND_DIFFUSE, NULL, add);
	}
	if (matchKind(kinds, LKIND_TYPE_SPOT | LKIND_MODE_BOTH)) {
		drawLight(title, "Spot A + D + pow(S)", 0, GU_SPOTLIGHT, GU_DIFFUSE_AND_SPECULAR, NULL, add);
	}
	if (matchKind(kinds, LKIND_TYPE_SPOT | LKIND_MODE_POWDIFFUSE)) {
		drawLight(title, "Spot A + pow(D)", 0, GU_SPOTLIGHT, GU_UNKNOWN_LIGHT_COMPONENT, NULL, add);
	}

	logBoxes();
}

static void testLightDiffuse() {
	ScePspFVector3 add = { 0.0f, 0.0f, 1.0f };

	resetLightState();

	checkpointNext("Diffuse 1.0");
	testLightTypes("  Diffuse 1.0", &add, LKIND_ALL);

	add.x = 0.5f;
	add.y = 0.5f;
	add.z = 0.0f;
	checkpointNext("Diffuse 0.0");
	testLightTypes("  Diffuse 0.0", &add, LKIND_ALL);

	add.x = 1.0f;
	add.y = 1.0f;
	add.z = 0.817f;
	checkpointNext("Diffuse 0.5");
	testLightTypes("  Diffuse 0.5", &add, LKIND_ALL);

	add.x = 1.0f;
	add.y = 1.0f;
	add.z = -0.817f;
	checkpointNext("Diffuse -0.5, specular 0.5");
	testLightTypes("  Diffuse -0.5", &add, LKIND_ALL);
}

static void testLightAttenuation() {
	ScePspFVector3 add = { 0.0f, 0.0f, 2.0f };
	ScePspFVector3 ldir111 = { 1.0f, 1.0f, 1.0f };
	ScePspFVector3 ldir001 = { 0.0f, 0.0f, 1.0f };

	// Should only affect spot + point: LKIND_TYPE_POINT | LKIND_TYPE_SPOT | LKIND_MODE_ALL
	// Using all kinds just to verify directional is unaffected.
	checkpointNext("Attenuation");
	resetLightState();

	add.x = 0.0f;
	add.y = 0.0f;
	add.z = 2.0f;
	sceGuLightAtt(0, 0.0f, 1.0f, 0.0f);
	testLightTypes("  Attenuation 0.5", &add, LKIND_ALL);
	sceGuLightAtt(0, 0.0f, 0.0f, 1.0f);
	testLightTypes("  Attenuation pow(0.5)", &add, LKIND_ALL);
	// The position isn't accurate so we can't get exactly zero easily, so use 0.001 to get an okay diffuse.
	add.x = 0.0f;
	add.y = 0.0f;
	add.z = 0.001f;
	sceGuLightAtt(0, 0.0f, 1.0f, 0.0f);
	// To see the spot.
	sceGuLightSpot(0, &ldir111, 1.0f, 0.0f);
	testLightTypes("  Attenuation large", &add, LKIND_ALL);

	add.x = 0.0f;
	add.y = 0.0f;
	add.z = 1000.0f;
	sceGuLightAtt(0, 0.0f, 1.0f, 0.0f);
	// Should be fine for the spot since distance is huge.
	sceGuLightSpot(0, &ldir001, 1.0f, 0.0f);
	testLightTypes("  Attenuation tiny", &add, LKIND_ALL);

	add.x = 0.0f;
	add.y = 0.0f;
	add.z = 1.0f;
	sceGuLightAtt(0, -1.0f, 0.0f, 0.0f);
	testLightTypes("  Attenuation -1.0", &add, LKIND_ALL);
}

static void testLightSpots() {
	ScePspFVector3 vec000 = { 0.0f, 0.0f, 0.0f };
	ScePspFVector3 vec001 = { 0.0f, 0.0f, 1.0f };
	ScePspFVector3 vec00N = { 0.0f, 0.0f, -1.0f };
	ScePspFVector3 vec100 = { 1.0f, 0.0f, 0.0f };
	ScePspFVector3 vec111 = { 1.0f, 1.0f, 1.0f };
	ScePspFVector3 vecNAN = { NAN, NAN, NAN };
	ScePspFVector3 vecNNAN = { -NAN, -NAN, -NAN };
	ScePspFVector3 vecINF = { INFINITY, INFINITY, INFINITY };
	ScePspFVector3 vecNINF = { -INFINITY, -INFINITY, -INFINITY };

	resetLightState();

	checkpointNext("Spot direction");
	sceGuLightSpot(0, &vec111, 1.0f, 0.0f);
	testLightTypes("  Spot z=1", &vec001, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	testLightTypes("  Spot z=-1", &vec00N, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	testLightTypes("  Spot xyz=1", &vec111, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vec001, 1.0f, 0.0f);
	testLightTypes("  Spot z=0", &vec100, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vec000, 1.0f, 0.0f);
	testLightTypes("  Spot zero vector", &vec111, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vecNAN, 1.0f, 0.0f);
	testLightTypes("  Spot NAN", &vec111, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vecNNAN, 1.0f, 0.0f);
	testLightTypes("  Spot -NAN", &vec111, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vecINF, 1.0f, 0.0f);
	testLightTypes("  Spot INFINITY", &vec111, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vecNINF, 1.0f, 0.0f);
	testLightTypes("  Spot -INFINITY", &vec111, LKIND_TYPE_SPOT | LKIND_MODE_ALL);

	checkpointNext("Spot exponent");
	sceGuLightSpot(0, &vec111, 2.0f, 0.0f);
	testLightTypes("  Spot e=2", &vec001, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vec111, 0.5f, 0.0f);
	testLightTypes("  Spot e=0.5", &vec001, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vec111, 0.0f, 0.0f);
	testLightTypes("  Spot e=0.0", &vec001, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vec000, 0.0f, 0.0f);
	testLightTypes("  Spot zero vector e=0.0", &vec001, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vec111, NAN, 0.0f);
	testLightTypes("  Spot e=NAN", &vec001, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vec111, -NAN, 0.0f);
	testLightTypes("  Spot e=-NAN", &vec001, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vec111, INFINITY, 0.0f);
	testLightTypes("  Spot e=INF", &vec001, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vec111, -INFINITY, 0.0f);
	testLightTypes("  Spot e=-INF", &vec001, LKIND_TYPE_SPOT | LKIND_MODE_ALL);

	checkpointNext("Spot cutoff");
	// Let's verify clamping...
	sceGuLightSpot(0, &vec00N, 0.0f, -100.0f);
	testLightTypes("  Spot negative cutoff + e=0.0", &vec001, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vec00N, 2.0f, -100.0f);
	testLightTypes("  Spot negative cutoff + e=2.0", &vec001, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vec001, 0.0f, 1.0f);
	testLightTypes("  Spot 1.0 cutoff=1.0", &vec001, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vec001, 0.0f, 2.0f);
	testLightTypes("  Spot 1.0 cutoff=2.0", &vec001, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vec001, 0.0f, -NAN);
	testLightTypes("  Spot 1.0 cutoff=-NAN", &vec001, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vec001, 0.0f, -INFINITY);
	testLightTypes("  Spot 1.0 cutoff=-INF", &vec001, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vec001, 0.0f, NAN);
	testLightTypes("  Spot 1.0 cutoff=NAN", &vec001, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
	sceGuLightSpot(0, &vec001, 0.0f, INFINITY);
	testLightTypes("  Spot 1.0 cutoff=INF", &vec001, LKIND_TYPE_SPOT | LKIND_MODE_ALL);
}

static void testPoweredDiffuse() {
	ScePspFVector3 vec000 = { 0.0f, 0.0f, 0.0f };
	ScePspFVector3 vec001 = { 0.0f, 0.0f, 1.0f };
	ScePspFVector3 vec00N = { 0.0f, 0.0f, -1.0f };
	ScePspFVector3 vec111 = { 1.0f, 1.0f, 1.0f };
	ScePspFVector3 diffuse05 = { 1.0f, 1.0f, 0.817f };

	resetLightState();
	checkpointNext("Powered diffuse");

	testLightTypes("  Diffuse 1.0 pow e=1.0", &vec001, LKIND_TYPE_ALL | LKIND_MODE_POWDIFFUSE);
	sceGuSpecular(0.0f);
	testLightTypes("  Diffuse 1.0 pow e=0.0", &vec001, LKIND_TYPE_ALL | LKIND_MODE_POWDIFFUSE);
	sceGuSpecular(0.0f);
	// To see the spot.
	sceGuLightSpot(0, &vec111, 0.0f, -100.0f);
	testLightTypes("  Diffuse -1.0 pow e=0.0", &vec00N, LKIND_TYPE_ALL | LKIND_MODE_POWDIFFUSE);
	sceGuSpecular(2.0f);
	testLightTypes("  Diffuse -1.0 pow e=2.0", &vec00N, LKIND_TYPE_ALL | LKIND_MODE_POWDIFFUSE);
	sceGuLightSpot(0, &vec001, 1.0f, 0.0f);
	sceGuSpecular(2.0f);
	testLightTypes("  Diffuse 0.5 pow e=2.0", &diffuse05, LKIND_TYPE_ALL | LKIND_MODE_POWDIFFUSE);
	sceGuSpecular(0.5f);
	testLightTypes("  Diffuse 0.5 pow e=0.5", &diffuse05, LKIND_TYPE_ALL | LKIND_MODE_POWDIFFUSE);
	sceGuSpecular(-1.0f);
	testLightTypes("  Diffuse 0.5 pow e=-1.0", &diffuse05, LKIND_TYPE_ALL | LKIND_MODE_POWDIFFUSE);
}

static void testSpecular() {
	ScePspFVector3 vec001 = { 0.0f, 0.0f, 1.0f };
	ScePspFVector3 vec111 = { 1.0f, 1.0f, 1.0f };
	ScePspFVector3 diffuse0 = { 0.5f, 0.5f, 0.0f };
	ScePspFVector3 diffuse05 = { 1.0f, 1.0f, 0.817f };
	ScePspFVector3 diffuseN = { 1.0f, 1.0f, -0.817f };

	resetLightState();
	checkpointNext("Specular");

	// To see the spot.
	sceGuLightSpot(0, &vec111, 0.0f, -100.0f);
	sceGuSpecular(0.0f);
	testLightTypes("  Diffuse 0.0 spec e=0.0", &diffuse0, LKIND_TYPE_ALL | LKIND_MODE_BOTH);
	testLightTypes("  Diffuse -0.5 spec e=0.0", &diffuseN, LKIND_TYPE_ALL | LKIND_MODE_BOTH);
	sceGuSpecular(0.5f);
	testLightTypes("  Diffuse 0.5 spec e=0.5", &diffuse05, LKIND_TYPE_ALL | LKIND_MODE_BOTH);
	sceGuSpecular(2.0f);
	testLightTypes("  Diffuse 0.5 spec e=2.0", &diffuse05, LKIND_TYPE_ALL | LKIND_MODE_BOTH);
	sceGuSpecular(-2.0f);
	testLightTypes("  Diffuse 0.5 spec e=-2.0", &diffuse05, LKIND_TYPE_ALL | LKIND_MODE_BOTH);
	boxNormZ = -1.0f;
	sceGuSpecular(0.0f);
	testLightTypes("  Diffuse 0.5 spec=-0.5 e=0.0", &diffuseN, LKIND_TYPE_ALL | LKIND_MODE_BOTH);
	sceGuSpecular(1.0f);
	testLightTypes("  Diffuse 0.5 spec=-0.5 e=1.0", &diffuseN, LKIND_TYPE_ALL | LKIND_MODE_BOTH);
	sceGuSpecular(2.0f);
	testLightTypes("  Diffuse 0.5 spec=-0.5 e=2.0", &diffuseN, LKIND_TYPE_ALL | LKIND_MODE_BOTH);
}

void draw() {
	startFrame();
	HAS_DISPLAY = 0;

	sceGuDisable(GU_TEXTURE);
	sceGuDisable(GU_BLEND);

	// Let's clear first to non-black so it's easier to tell.
	sceGuClearColor(0x003F3F3F);
	sceGuClearStencil(0);
	sceGuClear(GU_COLOR_BUFFER_BIT | GU_STENCIL_BUFFER_BIT);

	sceGuEnable(GU_LIGHTING);
	sceGuEnable(GU_LIGHT0);

	testLightDiffuse();
	testLightAttenuation();
	testLightSpots();
	testPoweredDiffuse();
	testSpecular();

	logBoxes();
	endFrame();
}

extern "C" int main(int argc, char *argv[]) {
	initDisplay();

	draw();

	sceGuTerm();

	return 0;
}
