#include "shared.h"

extern "C" int sceDmacMemcpy(void *dest, const void *source, unsigned int size);

static const int BOX_W = 40;
static const int BOX_H = 30;
static const int BOX_SPACE = 15;

static void nextBox(int t, int cw) {
	static int x = 10;
	static int y = 10;

	u32 col = 0xFF1F1FFF;

	Vertices v(t);
	switch (cw) {
	case 0:
		v.CP(col, x, y, 0);
		v.CP(col, x, y + BOX_H, 0);
		v.CP(col, x + BOX_W, y + BOX_H, 0);
		v.CP(col, x + BOX_W, y, 0);
		break;

	case 1:
		v.CP(col, x, y, 0);
		v.CP(col, x + BOX_W, y, 0);
		v.CP(col, x + BOX_W, y + BOX_H, 0);
		v.CP(col, x, y + BOX_H, 0);
		break;

	case 2:
		v.CP(col, x, y, 0);
		v.CP(col, x + BOX_W, y + BOX_H, 0);
		break;

	case 3:
		v.CP(col, x, y + BOX_H, 0);
		v.CP(col, x + BOX_W, y, 0);
		break;
	}

	void *p = sceGuGetMemory(v.Size());
	memcpy(p, v.Ptr(), v.Size());
	sceGuDrawArray(cw & 2 ? GU_SPRITES : GU_TRIANGLE_FAN, v.VType(), cw & 2 ? 2 : 4, NULL, p);

	x += BOX_W + BOX_SPACE;
	if (x + BOX_W >= 480) {
		x = 10;
		y += BOX_H + BOX_SPACE;
	}
}

static void cullBox(int transform, int clearval, int enable, int face, int cw) {
	if (enable) {
		sceGuEnable(GU_CULL_FACE);
	} else {
		sceGuDisable(GU_CULL_FACE);
	}
	sceGuSendCommandi(155, face);
	sceGuSendCommandi(211, clearval);
	nextBox(GU_COLOR_8888 | GU_VERTEX_16BIT | transform, cw);
	sceGuSendCommandi(211, 0);
}

static void logCullBox(const char *title, const char *name) {
	static int x = 10;
	static int y = 10;

	u32 buf[512] = { 0x13371337 };
	sceKernelDcacheWritebackInvalidateRange(buf, sizeof(buf));
	sceDmacMemcpy(buf, (uintptr_t)sceGeEdramGetAddr() + fbp0 + y * 512 * 4, 480 * 4);

	checkpoint("  %s (%s): %08x", title, name, buf[x]);

	x += BOX_W + BOX_SPACE;
	if (x + BOX_W >= 480) {
		x = 10;
		y += BOX_H + BOX_SPACE;
	}
}

static void cullBoxes(int transform, int clearVal) {
	cullBox(transform, clearVal, 0, 0, 1);
	cullBox(transform, clearVal, 0, 0, 0);
	cullBox(transform, clearVal, 1, 0, 1);
	cullBox(transform, clearVal, 1, 0, 0);
	cullBox(transform, clearVal, 1, 1, 1);
	cullBox(transform, clearVal, 1, 1, 0);
	cullBox(transform, clearVal, 1, 1, 2);
	cullBox(transform, clearVal, 1, 1, 3);
}

static void logCullBoxes(const char *title) {
	logCullBox(title, "0/0, cw");
	logCullBox(title, "0/0, ccw");
	logCullBox(title, "type=0, cw");
	logCullBox(title, "type=0, ccw");
	logCullBox(title, "type=1, cw");
	logCullBox(title, "type=1, ccw");
	logCullBox(title, "type=1, cw sprite");
	logCullBox(title, "type=1, ccw sprite");
}

void draw() {
	startFrame();

	sceGuDisable(GU_TEXTURE);
	sceGuDisable(GU_BLEND);

	// Let's clear first to non-black so it's easier to tell.
	sceGuClearColor(0x003F3F3F);
	sceGuClear(GU_COLOR_BUFFER_BIT);

	checkpointNext("Cull:");
	cullBoxes(GU_TRANSFORM_2D, 0);
	cullBoxes(GU_TRANSFORM_3D, 0);
	cullBoxes(GU_TRANSFORM_2D, 1 | (GU_COLOR_BUFFER_BIT << 8));
	cullBoxes(GU_TRANSFORM_3D, 1 | (GU_COLOR_BUFFER_BIT << 8));

	endFrame();

	logCullBoxes("Transform 2D");
	logCullBoxes("Transform 3D");
	logCullBoxes("Clear 2D");
	logCullBoxes("Clear 3D");
}

extern "C" int main(int argc, char *argv[]) {
	initDisplay();

	draw();

	emulatorEmitScreenshot();

	sceGuTerm();

	return 0;
}
