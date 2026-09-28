#include <common.h>
#include <pspdisplay.h>
#include <pspthreadman.h>

// After sceDisplayAdjustAccumulatedHcount(INT_MAX), how soon does the accumulated hcount wrap?

int sceDisplayGetCurrentHcount();
int sceDisplayGetAccumulatedHcount();
int sceDisplayAdjustAccumulatedHcount(int value);

static void busyWait(u32 us) {
	u32 start = sceKernelGetSystemTimeLow();
	while (sceKernelGetSystemTimeLow() - start < us) {
		continue;
	}
}

int main(int argc, char *argv[]) {
	static const int waits[] = { 0, 5, 10, 20, 30, 40, 50, 60, 70, 120 };
	checkpointNext("Accumulated hcount after adjusting to INT_MAX:");
	for (int w = 0; w < (int)ARRAY_SIZE(waits); ++w) {
		int wrapped = 0, notWrapped = 0;
		for (int i = 0; i < 20; ++i) {
			sceDisplayAdjustAccumulatedHcount(0x7FFFFFFF);
			int first = sceDisplayGetAccumulatedHcount();
			busyWait(waits[w]);
			int second = sceDisplayGetAccumulatedHcount();
			if (first != 0x7FFFFFFF) {
				schedf("  first read %d\n", first);
			}
			if (second < 5) {
				wrapped++;
			} else {
				notWrapped++;
			}
		}
		schedf("  %3dus later: wrapped %d, not %d\n", waits[w], wrapped, notWrapped);
	}
	flushschedf();
	return 0;
}
