#include <common.h>
#include <pspthreadman.h>

// Who runs next, and where a thread lands in the ready queue, after each way of making threads
// ready. Every thread appends its letter to a log when it gets the CPU; main appends M. Main runs
// at 0x20, and "drain" means main blocks for a while so every ready thread gets its turn.

// Semaphore wait queue ordered by priority rather than FIFO.
#define ATTR_PRIORITY 0x100

static char order[64];
static int orderLen = 0;

static void mark(char c) {
	if (orderLen < (int)sizeof(order) - 1) {
		order[orderLen++] = c;
		order[orderLen] = 0;
	}
}

static void reset() {
	orderLen = 0;
	order[0] = 0;
}

static void drain() {
	sceKernelDelayThread(5000);
}

static void report(const char *title) {
	schedf("  %s: %s\n", title, order);
	reset();
}

static SceUID sema, flag;

// argp: the letter.
static int semaWaiter(SceSize argc, void *argp) {
	char c = *(char *)argp;
	sceKernelWaitSema(sema, 1, NULL);
	mark(c);
	return 0;
}

static int flagWaiter(SceSize argc, void *argp) {
	char c = *(char *)argp;
	sceKernelWaitEventFlag(flag, 1, PSP_EVENT_WAITOR, NULL, NULL);
	mark(c);
	return 0;
}

static int sleeper(SceSize argc, void *argp) {
	char c = *(char *)argp;
	sceKernelSleepThread();
	mark(c);
	return 0;
}

static int runner(SceSize argc, void *argp) {
	char c = *(char *)argp;
	mark(c);
	return 0;
}

static SceUID threads[8];
static int threadCount = 0;

static SceUID start(SceKernelThreadEntry entry, char c, int prio) {
	SceUID t = sceKernelCreateThread("t", entry, prio, 0x1000, 0, NULL);
	sceKernelStartThread(t, 1, &c);
	threads[threadCount++] = t;
	return t;
}

static void cleanup() {
	for (int i = 0; i < threadCount; ++i) {
		sceKernelTerminateDeleteThread(threads[i]);
	}
	threadCount = 0;
	reset();
}

static void startWaiters(SceKernelThreadEntry entry, const char *letters, const int *prios) {
	for (int i = 0; letters[i]; ++i) {
		start(entry, letters[i], prios[i]);
	}
	// Let the better ones get to their waits; the worse and equal ones get there on drain.
	drain();
	reset();
}

static void testSema(const char *title, u32 attr, int count, const char *letters, const int *prios) {
	sema = sceKernelCreateSema("sema", attr, 0, 8, NULL);
	startWaiters(&semaWaiter, letters, prios);
	sceKernelSignalSema(sema, count);
	mark('M');
	drain();
	report(title);
	cleanup();
	sceKernelDeleteSema(sema);
}

static void testFlag(const char *title, u32 attr, const char *letters, const int *prios) {
	flag = sceKernelCreateEventFlag("flag", attr | PSP_EVENT_WAITMULTIPLE, 0, NULL);
	startWaiters(&flagWaiter, letters, prios);
	sceKernelSetEventFlag(flag, 1);
	mark('M');
	drain();
	report(title);
	cleanup();
	sceKernelDeleteEventFlag(flag);
}

int main(int argc, char *argv[]) {
	static const int worse[] = { 0x30, 0x28, 0x30 };
	static const int equal[] = { 0x20, 0x20, 0x20 };
	static const int better[] = { 0x18, 0x10, 0x18 };
	static const int mixed[] = { 0x18, 0x30, 0x20 };

	checkpointNext("Semaphore signalled for all three (A, B, C waited in that order):");
	testSema("FIFO, worse (0x30, 0x28, 0x30)", 0, 3, "ABC", worse);
	testSema("FIFO, equal", 0, 3, "ABC", equal);
	testSema("FIFO, better (0x18, 0x10, 0x18)", 0, 3, "ABC", better);
	testSema("FIFO, mixed (0x18, 0x30, 0x20)", 0, 3, "ABC", mixed);
	testSema("Priority attr, worse", ATTR_PRIORITY, 3, "ABC", worse);
	testSema("Priority attr, better", ATTR_PRIORITY, 3, "ABC", better);

	checkpointNext("Semaphore signalled for one:");
	testSema("FIFO, worse", 0, 1, "ABC", worse);
	testSema("Priority attr, worse", ATTR_PRIORITY, 1, "ABC", worse);
	testSema("FIFO, better", 0, 1, "ABC", better);
	testSema("Priority attr, better", ATTR_PRIORITY, 1, "ABC", better);

	checkpointNext("Event flag satisfying all three:");
	testFlag("FIFO, worse", 0, "ABC", worse);
	testFlag("FIFO, equal", 0, "ABC", equal);
	testFlag("FIFO, better", 0, "ABC", better);

	checkpointNext("Woken versus already ready, all at main's priority:");
	{
		// R is ready and hasn't run; W is sleeping. Main wakes W, then blocks.
		SceUID w = start(&sleeper, 'W', 0x20);
		drain();
		reset();
		start(&runner, 'R', 0x20);
		sceKernelWakeupThread(w);
		mark('M');
		drain();
		report("R started, then W woken");
		cleanup();

		w = start(&sleeper, 'W', 0x20);
		drain();
		reset();
		sceKernelWakeupThread(w);
		start(&runner, 'R', 0x20);
		mark('M');
		drain();
		report("W woken, then R started");
		cleanup();
	}

	checkpointNext("Starting threads at main's priority:");
	start(&runner, 'A', 0x20);
	start(&runner, 'B', 0x20);
	mark('M');
	drain();
	report("A, B started");
	cleanup();

	checkpointNext("sceKernelRotateThreadReadyQueue:");
	start(&runner, 'A', 0x20);
	start(&runner, 'B', 0x20);
	sceKernelRotateThreadReadyQueue(0);
	mark('M');
	drain();
	report("A, B ready, main rotates (0)");
	cleanup();

	start(&runner, 'A', 0x20);
	start(&runner, 'B', 0x20);
	sceKernelRotateThreadReadyQueue(0x20);
	mark('M');
	drain();
	report("A, B ready, main rotates (0x20)");
	cleanup();

	start(&runner, 'A', 0x30);
	start(&runner, 'B', 0x30);
	sceKernelRotateThreadReadyQueue(0x30);
	mark('M');
	drain();
	report("A, B ready at 0x30, main rotates (0x30)");
	cleanup();

	checkpointNext("sceKernelChangeThreadPriority:");
	{
		SceUID a = start(&runner, 'A', 0x20);
		start(&runner, 'B', 0x20);
		sceKernelChangeThreadPriority(a, 0x20);
		mark('M');
		drain();
		report("A, B ready, A changed to its own priority");
		cleanup();

		start(&runner, 'A', 0x20);
		start(&runner, 'B', 0x20);
		sceKernelChangeThreadPriority(0, 0x20);
		mark('M');
		drain();
		report("A, B ready, main changed to its own priority");
		cleanup();

		start(&runner, 'A', 0x20);
		sceKernelChangeThreadPriority(0, 0x21);
		mark('M');
		sceKernelChangeThreadPriority(0, 0x20);
		drain();
		report("A ready, main lowers itself to 0x21 and back");
		cleanup();

		a = start(&runner, 'A', 0x30);
		start(&runner, 'B', 0x28);
		sceKernelChangeThreadPriority(a, 0x28);
		mark('M');
		drain();
		report("A 0x30, B 0x28, A raised to 0x28");
		cleanup();
	}

	checkpointNext("sceKernelSuspendThread / sceKernelResumeThread:");
	{
		SceUID a = start(&runner, 'A', 0x20);
		start(&runner, 'B', 0x20);
		sceKernelSuspendThread(a);
		sceKernelResumeThread(a);
		mark('M');
		drain();
		report("A, B ready, A suspended and resumed");
		cleanup();
	}

	checkpointNext("sceKernelReleaseWaitThread:");
	{
		start(&runner, 'R', 0x20);
		SceUID w = start(&sleeper, 'W', 0x30);
		drain();
		reset();
		// W is 0x30 and sleeping; R has finished. Now make W equal to main and release it.
		start(&runner, 'R', 0x20);
		sceKernelChangeThreadPriority(w, 0x20);
		sceKernelReleaseWaitThread(w);
		mark('M');
		drain();
		report("R started, then W released");
		cleanup();
	}

	flushschedf();
	return 0;
}
