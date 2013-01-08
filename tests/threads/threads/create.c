#include "shared.h"

struct mem_entry {
	u32 start;
	u32 size;
	const char *name;
};

static struct mem_entry g_memareas[] = {
	{0x08800000, (24 * 1024 * 1024), "USER"},
	{0x48800000, (24 * 1024 * 1024), "USER XC"},
	{0x88000000, (4 * 1024 * 1024), "KERNEL LOW"},
	{0xA8000000, (4 * 1024 * 1024), "KERNEL LOW XC"},
	{0x88400000, (4 * 1024 * 1024), "KERNEL MID"},
	{0xC8400000, (4 * 1024 * 1024), "KERNEL MID XC"},
	{0x88800000, (24 * 1024 * 1024), "KERNEL HIGH"},
	{0xA8800000, (24 * 1024 * 1024), "KERNEL HIGH XC"},
	{0x04000000, (2 * 1024 * 1024), "VRAM"},
	{0x44000000, (2 * 1024 * 1024), "VRAM XC"},
	{0x00010000, (16 * 1024), "SCRATCHPAD"},
	{0x40010000, (16 * 1024), "SCRATCHPAD XC"},
	{0xBFC00000, (1 * 1024 * 1024), "INTERNAL"},
};

const char *ptrDesc(void *ptr) {
	u32 p = (u32) ptr;

	if (p == 0) {
		return "NULL";
	} else if (p == 0xDEADBEEF) {
		return "DEADBEEF";
	}

	int i;
	for (i = 0; i < sizeof(g_memareas) / sizeof(g_memareas[0]); ++i) {
		if (p >= g_memareas[i].start && p < g_memareas[i].start + g_memareas[i].size) {
			return g_memareas[i].name;
		}
	}
 
	return "UNKNOWN";
}

void schedfThreadStatus(SceUID thread) {
	SceKernelThreadInfo info;
	info.size = sizeof(info);

	int result = sceKernelReferThreadStatus(thread, &info);
	if (result >= 0) {
		schedf("OK (size=%d, name=%s, attr=%08x, status=%d, entry=%s, stack=%s, stackSize=%x,\n", info.size, info.name, info.attr, info.status, ptrDesc(info.entry), ptrDesc(info.stack), info.stackSize);
		schedf("    gpReg=%s, initPrio=%x, currPrio=%x, waitType=%d, waitId=%d, exit=%08X\n", ptrDesc(info.gpReg), info.initPriority, info.currentPriority, info.waitType, info.waitId, info.exitStatus);
		schedf("    run=%lld, intrPreempt=%u, threadPreempt=%u, release=%u\n", *(u64 *) &info.runClocks, info.intrPreemptCount, info.threadPreemptCount, info.releaseCount);
	} else {
		schedf("Invalid (%08X)\n", result);
	}
}

void testCreate(const char *title, const char *name, SceKernelThreadEntry entry, int prio, int stack, SceUInt attr, SceKernelThreadOptParam *opt) {
	SceUID result = sceKernelCreateThread(name, entry, prio, stack, attr, opt);
	if (result > 0) {
		schedf("%s: ", title);
		schedfThreadStatus(result);
		sceKernelDeleteThread(result);
	} else {
		schedf("%s: Failed (%08X)\n", title, result);
	}
}

static int testFunc(SceSize argc, void *argv) {
	return 0;
}

int main(int argc, char **argv) {
	int i;
	char temp[32];

	testCreate("Normal", "create", &testFunc, 0x20, 0x10000, 0, NULL);
	testCreate("NULL name", NULL, &testFunc, 0x20, 0x10000, 0, NULL);
	testCreate("Blank name", "", &testFunc, 0x20, 0x10000, 0, NULL);
	testCreate("Long name", "1234567890123456789012345678901234567890123456789012345678901234", &testFunc, 0x20, 0x10000, 0, NULL);

	flushschedf();

	u32 attrs[] = {1, 0x10, 0x50, 0x100, 0x600, 0xF00, 0x1000, 0x6000, 0xF000, 0x10000, 0xF0000, 0xF00000, 0x1000000, 0xF000000, 0x10000000, 0xF0000000};
	for (i = 0; i < sizeof(attrs) / sizeof(attrs[0]); ++i) {
		sprintf(temp, "0x%x attr", attrs[i]);
		testCreate(temp, "create", &testFunc, 0x20, 0x10000, attrs[i], NULL);
	}

	flushschedf();

	testCreate("Zero stack", "create", &testFunc, 0x20, 0, 0, NULL);
	int stacks[] = {-1, 1, -0x10000, 0x100, 0x1FF, 0x200, 0x1000, 0x100000, 0x1000000, 0x1000001, 0x20000000};
	for (i = 0; i < sizeof(stacks) / sizeof(stacks[0]); ++i) {
		sprintf(temp, "0x%08x stack", stacks[i]);
		testCreate(temp, "create", &testFunc, 0x20, stacks[i], 0, NULL);
	}

	flushschedf();

	int prios[] = {0, 0x06, 0x07, 0x08, 0x77, 0x78, -1};
	for (i = 0; i < sizeof(prios) / sizeof(prios[0]); ++i) {
		sprintf(temp, "0x%02x priority", prios[i]);
		testCreate(temp, "create", &testFunc, prios[i], 0x200, 0, NULL);
	}

	flushschedf();

	testCreate("Null entry", "create", NULL, 0x20, 0x10000, 0, NULL);
	testCreate("Invalid entry", "create", (SceKernelThreadEntry *) 0xDEADBEEF, 0x20, 0x10000, 0, NULL);

	flushschedf();

	SceUID thread1 = sceKernelCreateThread("create", &testFunc, 0x20, 0x1000, 0, NULL);
	SceUID thread2 = sceKernelCreateThread("create", &testFunc, 0x20, 0x1000, 0, NULL);
	if (thread1 > 0 && thread2 > 0) {
		schedf("Two with same name: OK\n");
	} else {
		schedf("Two with same name: Failed (%X, %X)\n", thread1, thread2);
	}
	sceKernelDeleteThread(thread1);
	sceKernelDeleteThread(thread2);

	flushschedf();

	SceUID thread;
	BASIC_SCHED_TEST("NULL name",
		thread = sceKernelCreateThread(NULL, &testFunc, 0x20, 0x1000, 0, NULL);
		result = thread > 0 ? 1 : thread;
	);
	BASIC_SCHED_TEST("Priority 0x70",
		thread = sceKernelCreateThread("create", &testFunc, 0x70, 0x1000, 0, NULL);
		result = thread > 0 ? 1 : thread;
	);
	sceKernelDeleteThread(thread);
	BASIC_SCHED_TEST("Priority 0x08",
		thread = sceKernelCreateThread("create", &testFunc, 0x08, 0x1000, 0, NULL);
		result = thread > 0 ? 1 : thread;
	);
	sceKernelDeleteThread(thread);

	// This also serves to see if stack space is committed on create.
	SceUID threads[1024];
	int result = 0;
	for (i = 0; i < 1024; ++i) {
		threads[i] = sceKernelCreateThread("create", &testFunc, 0x20, 0x0100000, 0, NULL);
		if (threads[i] < 0) {
			result = threads[i];
			break;
		}
	}

	if (result != 0) {
		schedf("Create 1024: Failed at %d (%08X)\n", i, result);
	} else {
		schedf("Create 1024: OK\n");
	}

	while (--i >= 0) {
		sceKernelDeleteThread(threads[i]);
	}

	flushschedf();

	return 0;
}