#include "shared.h"

// The syscall behind usersystemlib's sceKernelGetTlsAddr, which calls it as (uid, &addr, NULL) when
// the thread's cached address is null. The third argument is a timeout pointer.
extern "C" {
int _sceKernelAllocateTlspl(SceUID uid, void **addr, SceUInt *timeout);
int sceKernelSuspendDispatchThread();
int sceKernelResumeDispatchThread(int state);
}

static char *g_base = NULL;

static void schedfAddr(void *addr) {
	if (addr == NULL) {
		schedf("NULL");
	} else if (addr == (void *)0xDEADBEEF) {
		schedf("untouched");
	} else {
		schedf("+%04x", (int)((char *)addr - g_base));
	}
}

static void testAllocate(const char *title, SceUID uid, SceUInt *timeout = NULL) {
	void *addr = (void *)0xDEADBEEF;
	int result = _sceKernelAllocateTlspl(uid, &addr, timeout);
	checkpoint(NULL);
	schedf("%s: %08x, addr=", title, result);
	schedfAddr(addr);
	schedf(" ");
	schedfTlspl(uid);
}

struct AllocateThread : public KernelObjectWaitThread {
	AllocateThread(const char *name, SceUID uid, SceUInt timeout)
		: KernelObjectWaitThread(name, uid, timeout, 0x20) {
		start();
	}

	virtual int wait() {
		void *addr = (void *)0xDEADBEEF;
		int result;
		if (timeout_ == NO_TIMEOUT) {
			result = _sceKernelAllocateTlspl(object_, &addr, NULL);
		} else {
			SceUInt timeout = timeout_;
			result = _sceKernelAllocateTlspl(object_, &addr, &timeout);
			checkpoint("  ** %s timeout left: %s", name_, timeout == 0 ? "0" : (timeout < timeout_ ? "less" : "same"));
		}
		checkpoint(NULL);
		schedf("  ** %s got %08x, addr=", name_, result);
		schedfAddr(addr);
		if (addr != NULL && addr != (void *)0xDEADBEEF) {
			schedf(", first word=%08x", *(u32 *)addr);
		}
		schedf("\n");
		sceKernelSleepThread();
		return 0;
	}
};

extern "C" int main(int argc, char *argv[]) {
	SceUID tls = sceKernelCreateTlspl("tls", PSP_MEMORY_PARTITION_USER, 0, 0x10, 4, NULL);
	// A fresh pool hands out its first block first.
	g_base = (char *)sceKernelGetTlsAddr(tls);
	sceKernelFreeTlspl(tls);

	checkpointNext("Basic:");
	testAllocate("  First", tls);
	void *viaGet = sceKernelGetTlsAddr(tls);
	checkpoint(NULL);
	schedf("  sceKernelGetTlsAddr after: ");
	schedfAddr(viaGet);
	schedf("\n");
	memset(viaGet, 0xCC, 0x10);
	testAllocate("  Again", tls);
	checkpoint("  Not cleared again: %08x", *(u32 *)viaGet);
	sceKernelFreeTlspl(tls);
	testAllocate("  After free", tls);
	checkpoint("  Cleared: %08x", *(u32 *)viaGet);
	sceKernelFreeTlspl(tls);

	checkpointNext("Objects:");
	testAllocate("  Zero", 0);
	testAllocate("  Invalid", 0xDEADBEEF);
	testAllocate("  Negative", -1);
	// Same index bits, different uid.
	testAllocate("  Wrong uid, same index", tls ^ 0x100);
	SceUID deleted = sceKernelCreateTlspl("deleted", PSP_MEMORY_PARTITION_USER, 0, 0x10, 4, NULL);
	sceKernelDeleteTlspl(deleted);
	testAllocate("  Deleted", deleted);

	checkpointNext("Pointers:");
	{
		int result = _sceKernelAllocateTlspl(tls, (void **)0x88000000, NULL);
		checkpoint("  Kernel addr: %08x", result);
		SceUInt *kernelTimeout = (SceUInt *)0x88000000;
		testAllocate("  Kernel timeout", tls, kernelTimeout);
		sceKernelFreeTlspl(tls);
		SceUInt timeout = 100;
		testAllocate("  With timeout, not waiting", tls, &timeout);
		checkpoint("  Timeout left: %d", timeout);
		sceKernelFreeTlspl(tls);
	}

	checkpointNext("Dispatch:");
	{
		int state = sceKernelSuspendDispatchThread();
		testAllocate("  Dispatch suspended", tls);
		sceKernelResumeDispatchThread(state);
		testAllocate("  Dispatch resumed", tls);
		state = sceKernelSuspendDispatchThread();
		testAllocate("  Dispatch suspended, already allocated", tls);
		sceKernelResumeDispatchThread(state);
		sceKernelFreeTlspl(tls);

		int intr = sceKernelCpuSuspendIntr();
		void *addr = (void *)0xDEADBEEF;
		int result = _sceKernelAllocateTlspl(tls, &addr, NULL);
		sceKernelCpuResumeIntr(intr);
		checkpoint(NULL);
		schedf("  Interrupts disabled: %08x, addr=", result);
		schedfAddr(addr);
		schedf("\n");
		sceKernelFreeTlspl(tls);
	}

	sceKernelDeleteTlspl(tls);
	tls = sceKernelCreateTlspl("tls", PSP_MEMORY_PARTITION_USER, 0, 0x10, 1, NULL);
	g_base = (char *)sceKernelGetTlsAddr(tls);
	memset(g_base, 0xCC, 0x10);

	checkpointNext("Waiting:");
	{
		AllocateThread waiter("waiter", tls, NO_TIMEOUT);
		checkpoint(NULL);
		schedf("  While waiting: ");
		schedfTlspl(tls);
		checkpoint("  Free: %08x", sceKernelFreeTlspl(tls));
		sceKernelDelayThread(1000);
		checkpoint(NULL);
		schedf("  After handover: ");
		schedfTlspl(tls);
		// It still holds the block, and its thread ending returns it.
		sceKernelWakeupThread(waiter.thread_);
		sceKernelDelayThread(1000);
	}
	checkpoint(NULL);
	schedf("  After waiter ended: ");
	schedfTlspl(tls);

	checkpointNext("Timeout:");
	sceKernelGetTlsAddr(tls);
	{
		AllocateThread waiter("waiter", tls, 500);
		sceKernelDelayThread(10000);
		checkpoint(NULL);
		schedf("  After timeout: ");
		schedfTlspl(tls);
		sceKernelWakeupThread(waiter.thread_);
		sceKernelDelayThread(1000);
	}

	checkpointNext("Release:");
	{
		AllocateThread waiter("waiter", tls, NO_TIMEOUT);
		checkpoint("  Release: %08x", sceKernelReleaseWaitThread(waiter.thread_));
		sceKernelDelayThread(1000);
		sceKernelWakeupThread(waiter.thread_);
		sceKernelDelayThread(1000);
	}

	checkpointNext("Delete:");
	{
		AllocateThread waiter("waiter", tls, NO_TIMEOUT);
		checkpoint("  Delete: %08x", sceKernelDeleteTlspl(tls));
		sceKernelDelayThread(1000);
		sceKernelWakeupThread(waiter.thread_);
		sceKernelDelayThread(1000);
	}

	return 0;
}
