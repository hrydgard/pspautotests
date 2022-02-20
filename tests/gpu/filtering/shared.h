#pragma once

#include <common.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>

extern "C" int sceDmacMemcpy(void *dest, const void *source, unsigned int size);

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272

extern u32 __attribute__((aligned(16))) list[262144];

typedef u8 *GePtr;
extern GePtr fbp0;
extern GePtr dbp0;

void initDisplay();
void startFrame();
void endFrame();

void setNeedFull(bool v);

void clearDispBuffer(u32 c);
void dirtyDispBuffer();
u32 readDispBuffer();
u32 readFullDispBuffer(int x, int y);
