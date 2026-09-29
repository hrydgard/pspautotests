#include "shared.h"

// How long a Tlspl allocation that has to wait takes to time out, as threads/scheduling/waittimeouts
// does for the other kinds of wait. A pool of one block, held by another thread.

extern "C" int _sceKernelAllocateTlspl(SceUID uid, u32 *addr, SceUInt *timeout);

static SceUID tls;

static int holder(SceSize argc, void *argp) {
	sceKernelGetTlsAddr(tls);
	sceKernelSleepThread();
	return 0;
}

extern "C" int main(int argc, char *argv[]) {
	tls = sceKernelCreateTlspl("tls", PSP_MEMORY_PARTITION_USER, 0, 0x100, 1, NULL);
	SceUID thread = sceKernelCreateThread("holder", &holder, 0x10, 0x1000, 0, NULL);
	sceKernelStartThread(thread, 0, NULL);

	checkpointNext("_sceKernelAllocateTlspl timeouts (on time is within 10us of max(t, 205us) + 35us):");
	static const u32 timeouts[] = { 0, 1, 100, 205, 210, 220, 1000 };
	char line[256];
	int pos = sprintf(line, " ");
	int result = 0;
	for (size_t i = 0; i < ARRAY_SIZE(timeouts); ++i) {
		u32 best = 0xFFFFFFFF;
		SceUInt left = 0;
		for (int k = 0; k < 4; ++k) {
			SceUInt timeout = timeouts[i];
			u32 addr = 0;
			u32 start = sceKernelGetSystemTimeLow();
			result = _sceKernelAllocateTlspl(tls, &addr, &timeout);
			u32 t = sceKernelGetSystemTimeLow() - start;
			if (t < best) {
				best = t;
				left = timeout;
			}
		}
		u32 expected = (timeouts[i] > 205 ? timeouts[i] : 205) + 35;
		if (best < 50) {
			pos += sprintf(line + pos, " %d=at once", (int)timeouts[i]);
		} else if (best + 10 >= expected && best <= expected + 10) {
			pos += sprintf(line + pos, " %d=on time", (int)timeouts[i]);
		} else {
			pos += sprintf(line + pos, " %d=%dus", (int)timeouts[i], (int)best);
		}
		if (left != 0) {
			pos += sprintf(line + pos, "(left %d)", (int)left);
		}
	}
	schedf("%s, result %08x\n", line, result);

	sceKernelTerminateDeleteThread(thread);
	sceKernelDeleteTlspl(tls);
	flushschedf();
	return 0;
}
