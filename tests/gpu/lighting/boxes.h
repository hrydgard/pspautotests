#pragma once

#include <cstring>
#include <vector>
#include "shared.h"

// Small quads with one normal each, read back after the frame.

struct BoxVert {
	float nx, ny, nz;
	float x, y, z;
};

struct Box {
	int x;
	int y;
	u32 color;
};

static std::vector<Box> boxes;
static u32 __attribute__((aligned(16))) rowBuffer[512];

// Draws a 4x4 pixel quad with normal N around pixel (px, py), with the identity matrices in
// initDisplay() (or ones that cancel out).
static void drawBox(const float N[3]) {
	int i = (int)boxes.size();
	int px = 4 + (i % 40) * 12;
	int py = 4 + (i / 40) * 12;
	BoxVert *v = (BoxVert *)sceGuGetMemory(4 * sizeof(BoxVert));
	const float xs[4] = { -1.5f, 2.5f, 2.5f, -1.5f };
	const float ys[4] = { -1.5f, -1.5f, 2.5f, 2.5f };
	for (int j = 0; j < 4; ++j) {
		v[j].nx = N[0];
		v[j].ny = N[1];
		v[j].nz = N[2];
		v[j].x = (px + 0.5f + xs[j]) / 240.0f - 1.0f;
		v[j].y = 1.0f - (py + 0.5f + ys[j]) / 136.0f;
		v[j].z = 0.0f;
	}
	sceGuDrawArray(GU_TRIANGLE_FAN, GU_NORMAL_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, NULL, v);
	Box box = { px, py, 0 };
	boxes.push_back(box);
}

static void readBoxes() {
	for (size_t i = 0; i < boxes.size(); ++i) {
		sceKernelDcacheWritebackInvalidateRange(rowBuffer, sizeof(rowBuffer));
		sceDmacMemcpy(rowBuffer, (u8 *)sceGeEdramGetAddr() + (uintptr_t)fbp0 + boxes[i].y * 512 * 4, sizeof(rowBuffer));
		sceKernelDcacheWritebackInvalidateRange(rowBuffer, sizeof(rowBuffer));
		boxes[i].color = rowBuffer[boxes[i].x] & 0xFFFFFF;
	}
}

// Rotation about y by deg, as a view matrix; the projection undoes it so boxes stay in place.
static void setViewRotation(int deg) {
	float a = deg * (3.14159265f / 180.0f);
	float c = cosf(a), s = sinf(a);
	ScePspFMatrix4 view = {
		{ c, 0, -s, 0 },
		{ 0, 1, 0, 0 },
		{ s, 0, c, 0 },
		{ 0, 0, 0, 1 },
	};
	ScePspFMatrix4 proj = {
		{ c, 0, s, 0 },
		{ 0, 1, 0, 0 },
		{ -s, 0, c, 0 },
		{ 0, 0, 0, 1 },
	};
	sceGuSetMatrix(GU_VIEW, &view);
	sceGuSetMatrix(GU_PROJECTION, &proj);
}
