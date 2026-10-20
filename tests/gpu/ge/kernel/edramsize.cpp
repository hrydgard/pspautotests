#include <common.h>

// The VRAM size calls a PSP-2000 and later use to switch to their 4MB. Recorded on a PSP-1000,
// which only has 2MB.

extern "C" {
int sceGeEdramGetSize();
int sceGeEdramGetHwSize();
int sceGeEdramSetSize(int size);
}

extern "C" int main(int argc, char *argv[]) {
	checkpointNext("Sizes:");
	checkpoint("  sceGeEdramGetSize: %08x", sceGeEdramGetSize());
	checkpoint("  sceGeEdramGetHwSize: %08x", sceGeEdramGetHwSize());

	checkpointNext("sceGeEdramSetSize:");
	static const int sizes[] = { 0, 0x100000, 0x200000, 0x300000, 0x400000, -1 };
	for (size_t i = 0; i < ARRAY_SIZE(sizes); ++i) {
		const int result = sceGeEdramSetSize(sizes[i]);
		checkpoint("  %08x: %08x, size now %08x", sizes[i], result, sceGeEdramGetSize());
	}
	// Back to normal.
	sceGeEdramSetSize(0x200000);
	return 0;
}
