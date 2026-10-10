// A user module with some code, initialised data and bss, for unloadfill to look at after
// it's been unloaded.

#include <pspkernel.h>

PSP_MODULE_INFO("unloadfillchild", 0, 1, 1);

// Recognisable values, so it's clear whether they survive.
int data[64] = { 0x11111111, 0x22222222, 0x33333333, 0x44444444 };
int bss[64];

int module_start(SceSize args, void *argp) {
	int i;
	for (i = 0; i < 64; i++) {
		bss[i] = 0x55555555;
	}
	return 0;
}

int module_stop(SceSize args, void *argp) {
	return 0;
}
