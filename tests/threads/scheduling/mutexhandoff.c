#include <common.h>
#include <pspthreadman.h>

// Who gets a mutex when it's unlocked with several threads waiting, and whether the holder's
// priority is ever raised for a better waiter (priority inheritance). Waiters append their letter to
// a log when they get the lock; main appends U right after unlocking.

int sceKernelCreateMutex(const char *name, u32 attr, int count, void *options);
int sceKernelDeleteMutex(SceUID uid);
int sceKernelLockMutex(SceUID uid, int count, SceUInt *timeout);
int sceKernelUnlockMutex(SceUID uid, int count);

static char order[64];
static int orderLen = 0;
static SceUID mutex;
static SceLwMutexWorkarea lwmutex;
static int useLw = 0;

static void mark(char c) {
	if (orderLen < (int)sizeof(order) - 1) {
		order[orderLen++] = c;
		order[orderLen] = 0;
	}
}

static void report(const char *title) {
	schedf("  %s: %s\n", title, order);
	orderLen = 0;
	order[0] = 0;
}

static void lock() {
	if (useLw) {
		sceKernelLockLwMutex(&lwmutex, 1, NULL);
	} else {
		sceKernelLockMutex(mutex, 1, NULL);
	}
}

static void unlock() {
	if (useLw) {
		sceKernelUnlockLwMutex(&lwmutex, 1);
	} else {
		sceKernelUnlockMutex(mutex, 1);
	}
}

static int waiter(SceSize argc, void *argp) {
	char c = *(char *)argp;
	lock();
	mark(c);
	unlock();
	return 0;
}

static SceUID threads[4];

static void startWaiters(const char *letters, const int *prios) {
	for (int i = 0; letters[i]; ++i) {
		threads[i] = sceKernelCreateThread("waiter", &waiter, prios[i], 0x1000, 0, NULL);
		char c = letters[i];
		sceKernelStartThread(threads[i], 1, &c);
		// Let each one get to the lock, in this order, before the next starts.
		sceKernelDelayThread(1000);
	}
}

static void finishWaiters(int n) {
	for (int i = 0; i < n; ++i) {
		sceKernelWaitThreadEnd(threads[i], NULL);
		sceKernelDeleteThread(threads[i]);
	}
}

static void create(u32 attr) {
	if (useLw) {
		sceKernelCreateLwMutex(&lwmutex, "lwmutex", attr, 0, NULL);
	} else {
		mutex = sceKernelCreateMutex("mutex", attr, 0, NULL);
	}
}

static void destroy() {
	if (useLw) {
		sceKernelDeleteLwMutex(&lwmutex);
	} else {
		sceKernelDeleteMutex(mutex);
	}
}

static void testOrder(const char *title, u32 attr, const int *prios) {
	create(attr);
	lock();
	startWaiters("ABC", prios);
	orderLen = 0;
	order[0] = 0;
	unlock();
	mark('U');
	sceKernelDelayThread(5000);
	report(title);
	finishWaiters(3);
	destroy();
}

static void testInheritance(const char *title) {
	create(0);
	lock();
	int before = sceKernelGetThreadCurrentPriority();
	static const int better[] = { 0x10 };
	startWaiters("A", better);
	int during = sceKernelGetThreadCurrentPriority();
	orderLen = 0;
	order[0] = 0;
	unlock();
	finishWaiters(1);
	schedf("  %s: holder at 0x%02x, 0x%02x while a 0x10 thread waits\n", title, before, during);
	destroy();
}

int main(int argc, char *argv[]) {
	// A, B, C wait in that order.
	static const int worse[] = { 0x30, 0x28, 0x30 };
	static const int better[] = { 0x18, 0x10, 0x18 };
	static const int mixed[] = { 0x30, 0x10, 0x20 };

	for (useLw = 0; useLw < 2; ++useLw) {
		checkpointNext(useLw ? "LwMutex (A, B, C waited in that order):" : "Mutex (A, B, C waited in that order):");
		testOrder("FIFO, worse (0x30, 0x28, 0x30)", 0, worse);
		testOrder("Priority attr, worse", 0x100, worse);
		testOrder("FIFO, better (0x18, 0x10, 0x18)", 0, better);
		testOrder("Priority attr, better", 0x100, better);
		testOrder("FIFO, mixed (0x30, 0x10, 0x20)", 0, mixed);
		testOrder("Priority attr, mixed", 0x100, mixed);
		testInheritance("Inheritance");
	}

	flushschedf();
	return 0;
}
