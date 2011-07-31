#include <stdio.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <psploadexec.h>

PSP_MODULE_INFO("TEST GP", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

int gpValue = 1;

int main(int argc, char **argv) {
	Kprintf("%d", gpValue);
	gpValue = 2;
	Kprintf("%d", gpValue);

	return 0;
}