#include "shared.h"

int cbFunc(int arg1, int arg2, void *arg) {
	return 0;
}

void testCreate(const char *title, const char *name, SceKernelCallbackFunction func, void *arg) {
	SceUID cb = sceKernelCreateCallback(name, func, arg);
	if (cb >= 0) {
		checkpoint(NULL);
		schedf("%s: OK ", title);
		schedfCallback(cb);
		sceKernelDeleteCallback(cb);
	} else {
		checkpoint("%s: Failed (%08x)", title, cb);
	}
}

extern "C" int main(int argc, char *argv[]) {
	checkpointNext("Names:");
	testCreate("  NULL name", NULL, &cbFunc, NULL);
	testCreate("  Normal name", "test", &cbFunc, NULL);
	testCreate("  Blank name", "", &cbFunc, NULL);
	testCreate("  Long name", "1234567890123456789012345678901234567890123456789012345678901234", &cbFunc, NULL);

	checkpointNext("Funcs:");
	testCreate("  NULL func", "test", NULL, NULL);
	testCreate("  Invalid func", "test", (SceKernelCallbackFunction)0xDEADBEEF, NULL);
	testCreate("  Invalid func #2", "test", (SceKernelCallbackFunction)0x07ADBEEF, NULL);
	testCreate("  NULL func with arg", "test", NULL, (void *)0x1337);

	checkpointNext("Args:");
	testCreate("  Invalid arg pointer", "test", &cbFunc, (void *)0xDEADBEEF);

	return 0;
}