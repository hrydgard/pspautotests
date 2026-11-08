// A user module with no module_start or module_stop, and no entry point either (the ELF entry is
// 0xFFFFFFFF), like the libfont.prx some games ship on the disc.

#include <pspkernel.h>

PSP_MODULE_INFO("queryinfochild", 0, 1, 1);

int queryinfochild_value = 42;
