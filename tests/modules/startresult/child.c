// A user module whose module_start and module_stop return what the caller asks for.

#include <pspkernel.h>

#include "shared.h"

PSP_MODULE_INFO("startresultchild", 0, 1, 1);

int module_start(SceSize args, void *argp) {
	if (args != sizeof(Control *)) {
		return 0;
	}
	Control *control = *(Control **)argp;
	control->startCalls++;
	return control->startReturn;
}

int module_stop(SceSize args, void *argp) {
	if (args != sizeof(Control *)) {
		return 0;
	}
	Control *control = *(Control **)argp;
	control->stopCalls++;
	return control->stopReturn;
}
