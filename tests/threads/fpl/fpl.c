#include <common.h>

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <psploadexec.h>

typedef struct {
	int a[128];
} TestStruct;

void testSimpleFpl() {
	int n;
	SceUID fpl;
	TestStruct *item;
	
	printf("testSimpleFpl:\n");

	fpl = sceKernelCreateFpl("FPL", 2, 0, sizeof(TestStruct), 2, NULL);
	for (n = 0; n < 3; n++) {
		//item = NULL;
		printf("%08X: ", sceKernelTryAllocateFpl(fpl, (void **)&item));
		printf("%08X\n", (int)item);
	}
}

int main(int argc, char **argv) {
	testSimpleFpl();

	return 0;
}