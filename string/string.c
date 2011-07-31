#include <pspkernel.h>
#include <pspthreadman.h>
#include <stdio.h>
#include <string.h>

PSP_MODULE_INFO("STRING TEST", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

float value = 13.0;

int main(int argc, char** argv) {
	char buffer[128];
	
	sprintf(buffer, "%d", (u32)0);
	Kprintf("%s\n", buffer);
	
	sprintf(buffer, "%d", (u32)100000);
	Kprintf("%s\n", buffer);

	sprintf(buffer, "%lld", (u64)9);
	Kprintf("%s\n", buffer);

	sprintf(buffer, "%lld", (u64)100000);
	Kprintf("%s\n", buffer);

	sprintf(buffer, "%.2f", value);
	Kprintf("%s\n", buffer);

	sceKernelExitGame();
	return 0;
}
