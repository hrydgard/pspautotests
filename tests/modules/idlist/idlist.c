#include <common.h>
#include <pspmodulemgr.h>
#include <string.h>

// sceKernelGetModuleIdList: whether the size is in bytes or entries, what happens to a partial
// entry, and whether the count covers modules that didn't fit. The number of modules differs
// between a PSP (with PSPLink) and an emulator, so only relations get printed. Recorded on a PSP
// with plenty of modules loaded, so "ceil(size / 4)" there means exactly that for these sizes.

SceUID sceKernelGetModuleId(void);

#define SENTINEL 0xDEADBEEF

static SceUID buf[512];
static int total;

static int countWritten(int maxEntries) {
	int n = 0;
	for (int i = 0; i < maxEntries; ++i) {
		if ((u32)buf[i] != SENTINEL)
			n = i + 1;
	}
	return n;
}

static void testSize(const char *title, int size) {
	for (int i = 0; i < 512; ++i)
		buf[i] = (SceUID)SENTINEL;
	int count = -1;
	int result = sceKernelGetModuleIdList(buf, size, &count);
	int written = countWritten(64);
	// Rounded up to whole entries, but never more than there are modules.
	int expect = size <= 0 ? 0 : (size + 3) / 4;
	if (expect > total)
		expect = total;
	printf("%s: result=%08x, count is total: %d, entries written: %s\n", title, result, count == total,
		written == 0 ? "none" : written == expect ? "ceil(size / 4)" : "other");
}

int main(int argc, char **argv) {
	memset(buf, 0, sizeof(buf));
	int result = sceKernelGetModuleIdList(buf, sizeof(buf), &total);
	printf("Full buffer: result=%08x, total >= 1: %d, total fits: %d\n", result, total >= 1, total < 512);

	SceUID self = sceKernelGetModuleId();
	int found = 0;
	for (int i = 0; i < total && i < 512; ++i) {
		if (buf[i] == self)
			found = 1;
	}
	printf("Contains own module: %d\n", found);

	testSize("Size 0", 0);
	testSize("Size 1", 1);
	testSize("Size 3", 3);
	testSize("Size 4", 4);
	testSize("Size 5", 5);
	testSize("Size 6", 6);
	testSize("Size 7", 7);
	testSize("Size 8", 8);
	testSize("Size -4", -4);

	int count = -1;
	result = sceKernelGetModuleIdList(NULL, 0, &count);
	printf("NULL buffer, size 0: result=%08x, count is total: %d\n", result, count == total);
	count = -1;
	result = sceKernelGetModuleIdList(NULL, 4, &count);
	printf("NULL buffer, size 4: result=%08x, count is total: %d\n", result, count == total);

	return 0;
}
