/**
 * Dynamic recompilation implementations may reuse cached functions.
 * This tests that if the code memory changed, the code must be updated.
 */
//#pragma compile, "%PSPSDK%/bin/psp-gcc" -I. -I"%PSPSDK%/psp/sdk/include" -L. -L"%PSPSDK%/psp/sdk/lib" -D_PSP_FW_VERSION=150 -Wall -g change_cached_code.c ../common/emits.c -lpspsdk -lc -lpspuser -lpspkernel -o change_cached_code.elf
//#pragma compile, "%PSPSDK%/bin/psp-fixup-imports" change_cached_code.elf

#include <pspkernel.h>
#include <pspthreadman.h>
#include <string.h>
//#include <../common/emits.h>

PSP_MODULE_INFO("DYNAREC TEST", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

char test_dyna[1024];

void test1() { Kprintf("%d\n", 1); } void test1_end() { }
void test2() { Kprintf("%d\n", 2); } void test2_end() { }

int main() {
	memcpy(test_dyna, test1, test1_end - test1); ((void(*)(void))test_dyna)();
	memcpy(test_dyna, test2, test2_end - test2); ((void(*)(void))test_dyna)();
	
	sceKernelExitGame();
	
	return 0;
}