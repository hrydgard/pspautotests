#ifndef _COMMON_H
#define _COMMON_H 1

// Just to make IntelliSense happy, not attempting to compile.
#ifdef _MSC_VER
#define __builtin_va_list int
#include <stdarg.h>

#define __inline__ inline
#define __attribute__(...)
#define __extension__

#ifndef __builtin_va_start
void __va_start(va_list, ...);
void *__va_arg(va_list, ...);
void __va_end(va_list);

#define __builtin_va_start __va_start
#define __builtin_va_arg __va_arg
#define __builtin_va_end __va_end
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <pspctrl.h>

#undef main
#define main test_main

extern int RUNNING_ON_EMULATOR;

void emulatorEmitScreenshot();
void emulatorSendSceCtrlData(SceCtrlData* pad_data);

/*
void emitInt(int v);
void emitFloat(float v);
void emitString(char *v);
void emitComment(char *v);
void emitMemoryBlock(void *address, unsigned int size);
void emitHex(void *address, unsigned int size);
#define emitStringf(format, ...) { char temp[1024]; sprintf(temp, format, __VA_ARGS__); emitString(temp); }
*/

#endif