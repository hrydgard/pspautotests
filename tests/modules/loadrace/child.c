// A user module with a large initialised data section, so loading it takes a while.

#include <pspkernel.h>

PSP_MODULE_INFO("loadracechild", 0, 1, 1);

int data[65536] = { 0x11111111, 0x22222222, 0x33333333, 0x44444444 };

int module_start(SceSize args, void *argp) {
	return data[0] == 0x11111111 ? 0 : 1;
}

int module_stop(SceSize args, void *argp) {
	return 0;
}
