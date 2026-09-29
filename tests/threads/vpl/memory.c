#include "shared.h"

// The large-allocation half of the create test.
//
// Whether these succeed or come back with 80020190 depends on how much of the user partition is
// free when the test runs, which is a property of the setup that recorded the .expected, not of
// the API: PSPLink and the C library's heap take a large share over the cable. Don't put this in
// tests_good, and re-record it if it starts failing for that reason.
//
// The parameter checking that doesn't depend on free memory lives in create.c.

static void testCreate(const char *title, unsigned int size) {
	SceUID vpl = sceKernelCreateVpl("vpl", PSP_MEMORY_PARTITION_USER, 0, size, NULL);
	schedf("%s: ", title);
	schedfVpl(vpl);
	if (vpl > 0) {
		sceKernelDeleteVpl(vpl);
	}
}

int main(int argc, char **argv) {
	int i;
	char temp[128];

	schedf("Sizes:\n");
	unsigned int sizes[] = { 0x1000000, 0x1800000, 0x2000000 };
	for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
		sprintf(temp, "  Size 0x%08X", sizes[i]);
		testCreate(temp, sizes[i]);
	}
	flushschedf();

	SceUID vpls[1024];
	int result = 0;
	for (i = 0; i < 1024; i++) {
		vpls[i] = sceKernelCreateVpl("vpl", PSP_MEMORY_PARTITION_USER, 0, 0x1000, NULL);
		if (vpls[i] < 0) {
			result = vpls[i];
			break;
		}
	}

	if (result != 0) {
		printf("Create 1024: Failed at %d (%08X)\n", i, result);
	} else {
		printf("Create 1024: OK\n");
	}

	while (--i >= 0) {
		sceKernelDeleteVpl(vpls[i]);
	}

	return 0;
}
