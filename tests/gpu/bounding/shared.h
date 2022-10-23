#pragma once

#include <common.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>
#include "../commands/commands.h"

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

void clearDispBuffer(u32 c);
u32 readDispBuffer();
