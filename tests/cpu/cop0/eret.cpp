#include <common.h>

// eret: where it returns to with Status.EXL, ERL or both set, what it clears, and whether the
// instruction after it runs. Recorded with interrupts suspended, from a kernel module.

extern "C" {
int sceKernelCpuSuspendIntr();
void sceKernelCpuResumeIntr(int intr);
void EretTest(u32 statusBits, u32 *results);
}

extern "C" int main(int argc, char *argv[]) {
	static const struct {
		const char *name;
		u32 bits;
	} cases[] = {
		{ "EXL", 2 },
		{ "ERL", 4 },
		{ "EXL and ERL", 6 },
	};
	static const char *const targets[] = { "fell through", "EPC", "ErrorEPC" };

	for (size_t i = 0; i < ARRAY_SIZE(cases); ++i) {
		u32 results[3] = {};
		int intr = sceKernelCpuSuspendIntr();
		EretTest(cases[i].bits, results);
		sceKernelCpuResumeIntr(intr);
		checkpoint("%s: to %s, then EXL %d ERL %d, next instruction %s", cases[i].name,
			results[0] < 3 ? targets[results[0]] : "?", (results[1] >> 1) & 1, (results[1] >> 2) & 1,
			results[2] ? "ran" : "skipped");
	}
	return 0;
}
