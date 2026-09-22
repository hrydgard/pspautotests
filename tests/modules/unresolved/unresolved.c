#include <common.h>

// Calling an import the loader couldn't resolve. Emulators patch the stub with something of
// their own for this case, so what it returns (and that it returns at all) is worth pinning.

int bogusRtcCall(int a, int b);
int bogusRtcCall2(void);

int main(int argc, char *argv[]) {
	printf("bogus 1: %08x\n", bogusRtcCall(1, 2));
	printf("bogus 2: %08x\n", bogusRtcCall2());
	printf("bogus 1 again: %08x\n", bogusRtcCall(3, 4));
	printf("still alive\n");
	return 0;
}
