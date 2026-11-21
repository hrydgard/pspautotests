#include <common.h>
#include <pspiofilemgr.h>
#include <pspthreadman.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sysmem-imports.h>

// Async IO on ms0:: collecting results, what's allowed while one is pending or uncollected, the
// async open/close calls, and what sceIoClose does to an fd with an op queued, running, or finished
// but not collected. Also completion callbacks, the calls with dispatch disabled and in an interrupt,
// and the async thread's priority. Most cases run twice: with no compiled SDK version, and with one
// above 2.06, where the firmware refuses such a close with ASYNC_BUSY. Misshitsu no Sacrifice
// (NPJH00065/NPJH00077) relies on that: it calls sceIoReadAsync, then sceIoClose, and then
// collects the read with sceIoWaitAsync.

#define TESTFILE "ms0:/__asyncclose.bin"
#define TESTFILE2 "ms0:/__asyncclose2.bin"
#define MISSINGFILE "ms0:/__asyncclose_missing.bin"
#define FILESIZE 0x10000

static char buf[FILESIZE] __attribute__((aligned(64)));

// Output goes to a buffer, since printf blocks on a PSP and would let a queued op start. Line
// markers for reschedules are left out: exactly when a call reschedules isn't what this tests.
// Where a close blocks shows in the order the waiter threads' lines come out.
static void out(const char *format, ...) {
	va_list args;
	va_start(args, format);
	schedfBufferPos += vsprintf(schedfBuffer + schedfBufferPos, format, args);
	va_end(args);
	schedf("\n");
}

static void section(const char *title) {
	schedf("\n%s\n", title);
}

static void printRes(const char *title, int ret, SceInt64 res) {
	if (ret == 0 && strcmp(title, "Close") != 0) {
		out("  %s: %08x, res=%08x%08x", title, ret, (unsigned)(res >> 32), (unsigned)res);
	} else {
		out("  %s: %08x", title, ret);
	}
}

static void doWait(const char *title, SceUID fd) {
	SceInt64 res = -1;
	int ret = sceIoWaitAsync(fd, &res);
	printRes(title, ret, res);
}

static void doPoll(const char *title, SceUID fd) {
	SceInt64 res = -1;
	int ret = sceIoPollAsync(fd, &res);
	printRes(title, ret, res);
}

static void doStat(const char *title, SceUID fd, int poll) {
	SceInt64 res = -1;
	int ret = sceIoGetAsyncStat(fd, poll, &res);
	printRes(title, ret, res);
}

static void doWaitCB(const char *title, SceUID fd) {
	SceInt64 res = -1;
	int ret = sceIoWaitAsyncCB(fd, &res);
	printRes(title, ret, res);
}

// The async thread at a worse priority than ours doesn't run until we block, so the op stays queued.
static SceUID openQueued(const char *name) {
	SceUID fd = sceIoOpen(name, PSP_O_RDONLY, 0777);
	sceIoChangeAsyncPriority(fd, 0x70);
	return fd;
}

// The async thread at a better priority starts the op at once and blocks in the driver.
static SceUID openFast(const char *name) {
	SceUID fd = sceIoOpen(name, PSP_O_RDONLY, 0777);
	sceIoChangeAsyncPriority(fd, 0x10);
	return fd;
}

// Whatever a section left open, collect and close it quietly.
static void finish(SceUID fd) {
	SceInt64 res;
	sceIoWaitAsync(fd, &res);
	sceIoClose(fd);
}

static void checkData() {
	out("  Data read: %s", buf[1] == 1 ? "yes" : "no");
}

static SceUID waiterThread = -1;

enum { WAIT, POLL, STAT, WAITCB };

static int waiterFunc(SceSize argc, void *argp) {
	SceUID fd = ((int *)argp)[0];
	int mode = ((int *)argp)[1];
	SceInt64 res = -1;
	int ret;
	const char *title;
	switch (mode) {
	case WAIT: ret = sceIoWaitAsync(fd, &res); title = "Waiter Wait"; break;
	case POLL: ret = sceIoPollAsync(fd, &res); title = "Waiter Poll"; break;
	case STAT: ret = sceIoGetAsyncStat(fd, 0, &res); title = "Waiter GetAsyncStat"; break;
	default: ret = sceIoWaitAsyncCB(fd, &res); title = "Waiter WaitCB"; break;
	}
	// The result can be written even when the wait fails.
	out("  %s: %08x, res=%08x%08x", title, ret, (unsigned)(res >> 32), (unsigned)res);
	return 0;
}

static void startWaiterPri(SceUID fd, int mode, int pri) {
	int args[2] = { fd, mode };
	waiterThread = sceKernelCreateThread("waiter", &waiterFunc, pri, 0x1000, 0, NULL);
	sceKernelStartThread(waiterThread, sizeof(args), args);
}

// Better priority than us, so it's blocked before we continue.
static void startWaiter(SceUID fd, int mode) {
	startWaiterPri(fd, mode, sceKernelGetThreadCurrentPriority() - 1);
}

// Worse priority, so it only runs once we block.
static void startLateWaiter(SceUID fd, int mode) {
	startWaiterPri(fd, mode, sceKernelGetThreadCurrentPriority() + 1);
}

static void endWaiter() {
	SceUInt timeout = 500000;
	int ret = sceKernelWaitThreadEnd(waiterThread, &timeout);
	if (ret != 0) {
		out("  Waiter still waiting: %08x", ret);
		sceKernelTerminateThread(waiterThread);
	}
	sceKernelDeleteThread(waiterThread);
	waiterThread = -1;
}

static void createFiles() {
	int i;
	for (i = 0; i < FILESIZE; ++i) {
		buf[i] = (char)i;
	}
	SceUID fd = sceIoOpen(TESTFILE, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	sceIoWrite(fd, buf, FILESIZE);
	sceIoClose(fd);
	fd = sceIoOpen(TESTFILE2, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	sceIoWrite(fd, buf, 0x100);
	sceIoClose(fd);
}

static void removeFiles() {
	sceIoRemove(TESTFILE);
	sceIoRemove(TESTFILE2);
}

static void startRead(SceUID fd, int size) {
	memset(buf, 0xFF, FILESIZE);
	out("  ReadAsync: %08x", sceIoReadAsync(fd, buf, size));
}

static void closeQueuedWithWaiter(const char *title, int mode) {
	SceUID fd;
	section(title);
	fd = openQueued(TESTFILE);
	startRead(fd, 0x1000);
	startWaiter(fd, mode);
	out("  Close: %08x", sceIoClose(fd));
	sceKernelDelayThread(20000);
	endWaiter();
	checkData();
	finish(fd);
}

static void closeDoneThenCollect(const char *title, int mode) {
	SceUID fd;
	section(title);
	fd = openFast(TESTFILE);
	startRead(fd, 0x1000);
	sceKernelDelayThread(20000);
	out("  Close: %08x", sceIoClose(fd));
	switch (mode) {
	case WAIT: doWait("Wait", fd); break;
	case POLL: doPoll("Poll", fd); break;
	case STAT: doStat("GetAsyncStat", fd, 0); break;
	default: doWaitCB("WaitCB", fd); break;
	}
	doWait("Wait again", fd);
	out("  Close again: %08x", sceIoClose(fd));
	finish(fd);
}

static SceUID openQueuedMode(int flags) {
	SceUID fd = sceIoOpen(TESTFILE, flags, 0777);
	sceIoChangeAsyncPriority(fd, 0x70);
	return fd;
}

static SceUID openFastMode(int flags) {
	SceUID fd = sceIoOpen(TESTFILE, flags, 0777);
	sceIoChangeAsyncPriority(fd, 0x10);
	return fd;
}

// Run the sync calls against an fd with an op queued (pending) or done but not collected.
static void syncCalls(SceUID fd) {
	char small[16];
	out("  Read: %08x", sceIoRead(fd, small, sizeof(small)));
	out("  Write: %08x", sceIoWrite(fd, small, sizeof(small)));
	out("  Lseek: %08x", (int)sceIoLseek(fd, 0, PSP_SEEK_CUR));
	out("  Lseek32: %08x", sceIoLseek32(fd, 0, PSP_SEEK_CUR));
	out("  ReadAsync: %08x", sceIoReadAsync(fd, small, sizeof(small)));
	out("  LseekAsync: %08x", sceIoLseekAsync(fd, 0, PSP_SEEK_SET));
	out("  CloseAsync: %08x", sceIoCloseAsync(fd));
	out("  ChangeAsyncPriority: %08x", sceIoChangeAsyncPriority(fd, 0x10));
}

static void basicTests() {
	SceUID fd;
	SceInt64 res;

	section("No async op:");
	fd = openFastMode(PSP_O_RDONLY);
	doWait("Wait", fd);
	doPoll("Poll", fd);
	doStat("GetAsyncStat poll", fd, 1);
	doStat("GetAsyncStat wait", fd, 0);
	res = -1;
	printRes("WaitCB", sceIoWaitAsyncCB(fd, &res), res);
	sceIoClose(fd);

	section("Collecting twice:");
	fd = openFastMode(PSP_O_RDONLY);
	out("  ReadAsync: %08x", sceIoReadAsync(fd, buf, 0x1000));
	doWait("Wait", fd);
	doWait("Wait again", fd);
	doPoll("Poll", fd);
	sceIoClose(fd);

	section("Poll while queued:");
	fd = openQueuedMode(PSP_O_RDONLY);
	out("  ReadAsync: %08x", sceIoReadAsync(fd, buf, 0x1000));
	doPoll("Poll", fd);
	doStat("GetAsyncStat poll", fd, 1);
	doWait("Wait", fd);
	doPoll("Poll after", fd);
	sceIoClose(fd);

	section("GetAsyncStat collects:");
	fd = openFastMode(PSP_O_RDONLY);
	out("  ReadAsync: %08x", sceIoReadAsync(fd, buf, 0x1000));
	sceKernelDelayThread(20000);
	doStat("GetAsyncStat poll", fd, 1);
	doStat("GetAsyncStat poll again", fd, 1);
	out("  ReadAsync: %08x", sceIoReadAsync(fd, buf, 0x1000));
	doStat("GetAsyncStat wait", fd, 0);
	doStat("GetAsyncStat wait again", fd, 0);
	sceIoClose(fd);

	section("Calls while queued:");
	fd = openQueuedMode(PSP_O_RDWR);
	out("  ReadAsync: %08x", sceIoReadAsync(fd, buf, 0x1000));
	syncCalls(fd);
	doWait("Wait", fd);
	sceIoClose(fd);

	section("Calls with an uncollected result:");
	fd = openFastMode(PSP_O_RDWR);
	out("  ReadAsync: %08x", sceIoReadAsync(fd, buf, 0x1000));
	sceKernelDelayThread(20000);
	syncCalls(fd);
	doWait("Wait", fd);
	sceIoClose(fd);

	section("Read position after async read:");
	fd = openFastMode(PSP_O_RDONLY);
	out("  ReadAsync: %08x", sceIoReadAsync(fd, buf, 0x1000));
	doWait("Wait", fd);
	out("  Lseek: %08x", (int)sceIoLseek(fd, 0, PSP_SEEK_CUR));
	out("  LseekAsync: %08x", sceIoLseekAsync(fd, 0x10, PSP_SEEK_CUR));
	doWait("Wait", fd);
	out("  Lseek32Async: %08x", sceIoLseek32Async(fd, 0x10, PSP_SEEK_CUR));
	doWait("Wait", fd);
	out("  ReadAsync at end: %08x", sceIoReadAsync(fd, buf, FILESIZE));
	doWait("Wait", fd);
	out("  ReadAsync past end: %08x", sceIoReadAsync(fd, buf, FILESIZE));
	doWait("Wait", fd);
	sceIoClose(fd);

	section("Wrong access mode:");
	fd = openFastMode(PSP_O_RDONLY);
	out("  WriteAsync: %08x", sceIoWriteAsync(fd, buf, 0x10));
	doWait("Wait", fd);
	sceIoClose(fd);
	fd = openFastMode(PSP_O_WRONLY);
	out("  ReadAsync: %08x", sceIoReadAsync(fd, buf, 0x10));
	doWait("Wait", fd);
	sceIoClose(fd);

	section("CloseAsync:");
	fd = openFastMode(PSP_O_RDONLY);
	out("  CloseAsync: %08x", sceIoCloseAsync(fd));
	doWait("Wait", fd);
	doWait("Wait again", fd);
	out("  Close: %08x", sceIoClose(fd));

	section("CloseAsync, poll:");
	fd = openQueuedMode(PSP_O_RDONLY);
	out("  CloseAsync: %08x", sceIoCloseAsync(fd));
	doPoll("Poll", fd);
	out("  Read: %08x", sceIoRead(fd, buf, 0x10));
	doWait("Wait", fd);
	doPoll("Poll again", fd);

	section("CloseAsync, then Close:");
	fd = openFastMode(PSP_O_RDONLY);
	out("  CloseAsync: %08x", sceIoCloseAsync(fd));
	sceKernelDelayThread(20000);
	out("  Close: %08x", sceIoClose(fd));
	doWait("Wait", fd);

	section("CloseAsync, then Close while queued:");
	fd = openQueuedMode(PSP_O_RDONLY);
	out("  CloseAsync: %08x", sceIoCloseAsync(fd));
	out("  Close: %08x", sceIoClose(fd));
	doWait("Wait", fd);

	section("OpenAsync:");
	fd = sceIoOpenAsync(TESTFILE, PSP_O_RDONLY, 0777);
	out("  OpenAsync: %s", fd >= 0 ? "fd" : "error");
	res = -1;
	{
		int ret = sceIoWaitAsync(fd, &res);
		out("  Wait: %08x, res %s", ret, res == fd ? "= fd" : "!= fd");
	}
	doWait("Wait again", fd);
	out("  Read: %08x", sceIoRead(fd, buf, 0x10));
	out("  Close: %08x", sceIoClose(fd));

	section("OpenAsync, missing file:");
	fd = sceIoOpenAsync(MISSINGFILE, PSP_O_RDONLY, 0777);
	out("  OpenAsync: %s", fd >= 0 ? "fd" : "error");
	doPoll("Poll", fd);
	doWait("Wait", fd);
	doWait("Wait again", fd);
	out("  Read: %08x", sceIoRead(fd, buf, 0x10));
	out("  Close: %08x", sceIoClose(fd));

	section("OpenAsync, bad device:");
	fd = sceIoOpenAsync("nodev0:/foo", PSP_O_RDONLY, 0777);
	out("  OpenAsync: %08x", fd);

	section("Invalid fds:");
	// Not 0-2: PSPLink has its own files behind those.
	doWait("Wait -1", -1);
	doWait("Wait 20", 20);
	doWait("Wait 63", 63);
	doWait("Wait 64", 64);
	doPoll("Poll 20", 20);
	doStat("GetAsyncStat 20", 20, 1);
	out("  ReadAsync 20: %08x", sceIoReadAsync(20, buf, 0x10));
	out("  CloseAsync 20: %08x", sceIoCloseAsync(20));
	out("  ChangeAsyncPriority 20: %08x", sceIoChangeAsyncPriority(20, 0x20));
	out("  ChangeAsyncPriority 64: %08x", sceIoChangeAsyncPriority(64, 0x20));

	section("ChangeAsyncPriority values:");
	fd = openFastMode(PSP_O_RDONLY);
	{
		static const int pris[] = { -2, -1, 0, 1, 7, 8, 0x77, 0x78, 0x7F };
		int i;
		for (i = 0; i < (int)ARRAY_SIZE(pris); ++i) {
			out("  %d: %08x", pris[i], sceIoChangeAsyncPriority(fd, pris[i]));
		}
	}
	sceIoClose(fd);
}

static void nullResultTests() {
	SceUID fd;

	section("NULL result pointer:");
	fd = openFastMode(PSP_O_RDONLY);
	out("  Wait, no op: %08x", sceIoWaitAsync(fd, NULL));
	out("  ReadAsync: %08x", sceIoReadAsync(fd, buf, 0x10));
	out("  Poll: %08x", sceIoPollAsync(fd, NULL));
	out("  Wait: %08x", sceIoWaitAsync(fd, NULL));
	doWait("Wait again", fd);
	sceIoClose(fd);
}

static void closeTests() {
	SceUID fd, fd2;

	section("Close while queued:");
	fd = openQueued(TESTFILE);
	startRead(fd, 0x1000);
	out("  Close: %08x", sceIoClose(fd));
	doPoll("Poll", fd);
	doWait("Wait", fd);
	checkData();
	doWait("Wait again", fd);
	out("  Close again: %08x", sceIoClose(fd));
	finish(fd);

	closeQueuedWithWaiter("Close while queued, Wait waiter:", WAIT);
	closeQueuedWithWaiter("Close while queued, Poll waiter:", POLL);
	closeQueuedWithWaiter("Close while queued, GetAsyncStat waiter:", STAT);
	closeQueuedWithWaiter("Close while queued, WaitCB waiter:", WAITCB);

	section("Close while in progress:");
	fd = openFast(TESTFILE);
	startRead(fd, FILESIZE);
	out("  Close: %08x", sceIoClose(fd));
	doPoll("Poll", fd);
	doWait("Wait", fd);
	// Not before: PPSSPP reads on a host thread, so a half-done read is timing dependent.
	checkData();
	out("  Close again: %08x", sceIoClose(fd));
	finish(fd);

	section("Close while in progress, waiter:");
	fd = openFast(TESTFILE);
	startRead(fd, FILESIZE);
	startWaiter(fd, WAIT);
	out("  Close: %08x", sceIoClose(fd));
	endWaiter();
	checkData();
	finish(fd);

	section("Close while in progress, late waiter:");
	fd = openFast(TESTFILE);
	startRead(fd, FILESIZE);
	startLateWaiter(fd, WAIT);
	out("  Close: %08x", sceIoClose(fd));
	endWaiter();
	finish(fd);

	section("Close while in progress, late poller:");
	fd = openFast(TESTFILE);
	startRead(fd, FILESIZE);
	startLateWaiter(fd, POLL);
	out("  Close: %08x", sceIoClose(fd));
	endWaiter();
	finish(fd);

	closeDoneThenCollect("Close after completion, Wait:", WAIT);
	closeDoneThenCollect("Close after completion, Poll:", POLL);
	closeDoneThenCollect("Close after completion, GetAsyncStat:", STAT);
	closeDoneThenCollect("Close after completion, WaitCB:", WAITCB);

	section("Close after collecting:");
	fd = openFast(TESTFILE);
	startRead(fd, 0x1000);
	doWait("Wait", fd);
	out("  Close: %08x", sceIoClose(fd));
	doWait("Wait", fd);
	doPoll("Poll", fd);

	section("Close without async:");
	fd = openFast(TESTFILE);
	out("  Close: %08x", sceIoClose(fd));
	doWait("Wait", fd);

	section("Reopen after close:");
	fd = openFast(TESTFILE);
	startRead(fd, 0x1000);
	sceKernelDelayThread(20000);
	out("  Close: %08x", sceIoClose(fd));
	fd2 = sceIoOpen(TESTFILE2, PSP_O_RDONLY, 0777);
	out("  Open other: %s", fd2 == fd ? "same fd" : "different fd");
	doWait("Wait on new fd", fd2);
	out("  Read new: %08x", sceIoRead(fd2, buf, 0x1000));
	out("  Close new: %08x", sceIoClose(fd2));
	doWait("Wait on old fd", fd);
	finish(fd);

	section("Seek result after close:");
	fd = openFast(TESTFILE);
	out("  LseekAsync: %08x", sceIoLseekAsync(fd, 0x1234, PSP_SEEK_SET));
	sceKernelDelayThread(20000);
	out("  Close: %08x", sceIoClose(fd));
	doWait("Wait", fd);
	finish(fd);
}

static volatile int cbCount = 0;
static volatile int cbArg2 = 0;
static volatile int cbCommon = 0;

static int asyncCallback(int arg1, int arg2, void *common) {
	cbCount++;
	cbArg2 = arg2;
	cbCommon = (int)common;
	return 0;
}

static void printCallback(const char *title) {
	out("  %s: %d calls, arg2=%08x, common=%08x", title, cbCount, cbArg2, cbCommon);
	cbCount = 0;
	cbArg2 = 0;
	cbCommon = 0;
}

static void callbackTests() {
	SceUID fd, sema;
	SceUID cb = sceKernelCreateCallback("asynccb", &asyncCallback, (void *)0x1234);

	section("Async callback:");
	fd = openFastMode(PSP_O_RDONLY);
	out("  SetAsyncCallback: %08x", sceIoSetAsyncCallback(fd, cb, (void *)0x55));
	out("  ReadAsync: %08x", sceIoReadAsync(fd, buf, 0x10));
	doWait("Wait", fd);
	printCallback("After Wait");
	out("  CheckCallback: %08x", sceKernelCheckCallback());
	printCallback("After CheckCallback");
	out("  ReadAsync: %08x", sceIoReadAsync(fd, buf, 0x10));
	doWaitCB("WaitCB", fd);
	printCallback("After WaitCB");
	out("  ReadAsync: %08x", sceIoReadAsync(fd, buf, 0x10));
	sceKernelDelayThread(20000);
	printCallback("After Delay");
	sceKernelDelayThreadCB(1000);
	printCallback("After DelayCB");
	doPoll("Poll", fd);
	out("  CloseAsync: %08x", sceIoCloseAsync(fd));
	doWaitCB("WaitCB", fd);
	printCallback("After CloseAsync");

	section("Async callback, queued op:");
	fd = openQueuedMode(PSP_O_RDONLY);
	out("  SetAsyncCallback: %08x", sceIoSetAsyncCallback(fd, cb, (void *)0x66));
	out("  ReadAsync: %08x", sceIoReadAsync(fd, buf, 0x10));
	doWaitCB("WaitCB", fd);
	printCallback("After WaitCB");
	out("  SetAsyncCallback 0: %08x", sceIoSetAsyncCallback(fd, 0, (void *)0x77));
	out("  ReadAsync: %08x", sceIoReadAsync(fd, buf, 0x10));
	doWaitCB("WaitCB", fd);
	printCallback("After WaitCB");
	sceIoClose(fd);

	section("SetAsyncCallback, bad args:");
	fd = openFastMode(PSP_O_RDONLY);
	sema = sceKernelCreateSema("notcb", 0, 0, 1, NULL);
	out("  Not a callback: %08x", sceIoSetAsyncCallback(fd, sema, NULL));
	out("  Bad uid: %08x", sceIoSetAsyncCallback(fd, 0x12345, NULL));
	out("  -1: %08x", sceIoSetAsyncCallback(fd, -1, NULL));
	out("  Bad fd: %08x", sceIoSetAsyncCallback(20, cb, NULL));
	sceKernelDeleteSema(sema);
	sceIoClose(fd);

	sceKernelDeleteCallback(cb);
}

enum { STAT_POLL = WAITCB + 1, CLOSE };

static const char *modeName(int mode) {
	switch (mode) {
	case WAIT: return "Wait";
	case POLL: return "Poll";
	case STAT: return "GetAsyncStat wait";
	case STAT_POLL: return "GetAsyncStat poll";
	case WAITCB: return "WaitCB";
	default: return "Close";
	}
}

static int callMode(SceUID fd, int mode, SceInt64 *res) {
	switch (mode) {
	case WAIT: return sceIoWaitAsync(fd, res);
	case POLL: return sceIoPollAsync(fd, res);
	case STAT: return sceIoGetAsyncStat(fd, 0, res);
	case STAT_POLL: return sceIoGetAsyncStat(fd, 1, res);
	case WAITCB: return sceIoWaitAsyncCB(fd, res);
	default: return sceIoClose(fd);
	}
}

static void dispatchCall(SceUID fd, int mode) {
	SceInt64 res = -1;
	int state = sceKernelSuspendDispatchThread();
	int ret = callMode(fd, mode, &res);
	sceKernelResumeDispatchThread(state);
	printRes(modeName(mode), ret, res);
}

static void dispatchTests() {
	SceUID fd;
	int mode, state, state2;

	section("Dispatch disabled, result ready:");
	for (mode = WAIT; mode <= CLOSE; ++mode) {
		fd = openFastMode(PSP_O_RDONLY);
		sceIoReadAsync(fd, buf, 0x10);
		sceKernelDelayThread(20000);
		dispatchCall(fd, mode);
		finish(fd);
	}

	section("Dispatch disabled, op queued:");
	for (mode = WAIT; mode <= CLOSE; ++mode) {
		fd = openQueuedMode(PSP_O_RDONLY);
		sceIoReadAsync(fd, buf, 0x10);
		dispatchCall(fd, mode);
		finish(fd);
	}

	section("Dispatch disabled, op running:");
	fd = openFastMode(PSP_O_RDONLY);
	sceIoReadAsync(fd, buf, FILESIZE);
	dispatchCall(fd, CLOSE);
	finish(fd);

	section("Dispatch disabled, starting an op:");
	for (mode = 0; mode < 5; ++mode) {
		int ret;
		const char *name;
		fd = sceIoOpen(TESTFILE2, PSP_O_RDWR, 0777);
		sceIoChangeAsyncPriority(fd, 0x10);
		state = sceKernelSuspendDispatchThread();
		switch (mode) {
		case 0: ret = sceIoReadAsync(fd, buf, 0x10); name = "ReadAsync"; break;
		case 1: ret = sceIoWriteAsync(fd, buf, 0x10); name = "WriteAsync"; break;
		case 2: ret = sceIoLseekAsync(fd, 0, PSP_SEEK_SET); name = "LseekAsync"; break;
		case 3: ret = sceIoLseek32Async(fd, 0, PSP_SEEK_SET); name = "Lseek32Async"; break;
		default: ret = sceIoCloseAsync(fd); name = "CloseAsync"; break;
		}
		state2 = sceKernelSuspendDispatchThread();
		sceKernelResumeDispatchThread(state);
		out("  %s: %08x, dispatch after: %s", name, ret, state2 == state ? "enabled" : "disabled");
		finish(fd);
	}
	// Put back what WriteAsync wrote.
	createFiles();

	section("Dispatch disabled, OpenAsync:");
	state = sceKernelSuspendDispatchThread();
	fd = sceIoOpenAsync(TESTFILE, PSP_O_RDONLY, 0777);
	state2 = sceKernelSuspendDispatchThread();
	sceKernelResumeDispatchThread(state);
	out("  OpenAsync: %s", fd >= 0 ? "fd" : "error");
	out("  Dispatch after: %s", state2 == state ? "enabled" : "disabled");
	finish(fd);
}

static volatile int intrMode = 0;
static volatile int intrRet = 0;
static volatile int intrDone = 0;
static SceUID intrFd = -1;
static SceInt64 intrRes = -1;

static SceUInt intrHandler(void *common) {
	intrRet = callMode(intrFd, intrMode, &intrRes);
	intrDone = 1;
	return 0;
}

// Spins rather than blocks, so a queued op doesn't get to run.
static void intrCall(SceUID fd, int mode) {
	intrFd = fd;
	intrMode = mode;
	intrRes = -1;
	intrDone = 0;
	sceKernelSetAlarm(1000, &intrHandler, NULL);
	while (!intrDone) {
		continue;
	}
	printRes(modeName(mode), intrRet, intrRes);
}

static void interruptTests() {
	SceUID fd;
	int mode;

	section("In an interrupt, result ready:");
	for (mode = WAIT; mode <= CLOSE; ++mode) {
		fd = openFastMode(PSP_O_RDONLY);
		sceIoReadAsync(fd, buf, 0x10);
		sceKernelDelayThread(20000);
		intrCall(fd, mode);
		doPoll("Poll after", fd);
		finish(fd);
	}

	section("In an interrupt, no op:");
	fd = openFastMode(PSP_O_RDONLY);
	intrCall(fd, POLL);
	intrCall(fd, CLOSE);
	out("  Close after: %08x", sceIoClose(fd));

	section("In an interrupt, op queued:");
	for (mode = WAIT; mode <= CLOSE; ++mode) {
		fd = openQueuedMode(PSP_O_RDONLY);
		sceIoReadAsync(fd, buf, 0x10);
		intrCall(fd, mode);
		doWait("Wait after", fd);
		finish(fd);
	}
}

// User mode can't see the async threads, so the priority shows in whether an op runs while the
// thread that started it spins: only an async thread with a better priority gets to run.
static SceUID probeFd = -1;
static int probeRet = 0;

static int probeFunc(SceSize argc, void *argp) {
	SceInt64 res;
	u32 start;
	sceIoReadAsync(probeFd, buf, 0x10);
	start = sceKernelGetSystemTimeLow();
	while (sceKernelGetSystemTimeLow() - start < 20000) {
		continue;
	}
	probeRet = sceIoPollAsync(probeFd, &res);
	return 0;
}

static void probe(const char *title, SceUID fd, int pri) {
	SceInt64 res;
	SceUID th = sceKernelCreateThread("probe", &probeFunc, pri, 0x1000, 0, NULL);
	probeFd = fd;
	sceKernelStartThread(th, 0, NULL);
	sceKernelWaitThreadEnd(th, NULL);
	sceKernelDeleteThread(th);
	out("  %s: %s", title, probeRet == 0 ? "ran" : probeRet == 1 ? "didn't run" : "error");
	sceIoWaitAsync(fd, &res);
}

static void priorityTests() {
	SceUID fd;
	int main = sceKernelGetThreadCurrentPriority();

	section("Async priority:");
	fd = sceIoOpen(TESTFILE, PSP_O_RDONLY, 0777);
	probe("Default, first op from +10", fd, main + 0x10);
	probe("Default, then from +20", fd, main + 0x20);
	probe("Default, then from +8", fd, main + 0x08);
	sceIoClose(fd);

	fd = sceIoOpen(TESTFILE, PSP_O_RDONLY, 0777);
	probe("Default, first op from main", fd, main);
	probe("Default, then from +10", fd, main + 0x10);
	sceIoClose(fd);

	fd = sceIoOpen(TESTFILE, PSP_O_RDONLY, 0777);
	out("  Set 0x40: %08x", sceIoChangeAsyncPriority(fd, 0x40));
	probe("0x40, from 0x38", fd, 0x38);
	probe("0x40, from 0x48", fd, 0x48);
	out("  Set -1: %08x", sceIoChangeAsyncPriority(fd, -1));
	probe("-1, from +10", fd, main + 0x10);
	probe("-1, from +20", fd, main + 0x20);
	sceIoClose(fd);

	out("  Set default 0x40: %08x", sceIoChangeAsyncPriority(-1, 0x40));
	fd = sceIoOpen(TESTFILE, PSP_O_RDONLY, 0777);
	probe("Default 0x40, from 0x38", fd, 0x38);
	probe("Default 0x40, from 0x48", fd, 0x48);
	sceIoClose(fd);
	out("  Set default -1: %08x", sceIoChangeAsyncPriority(-1, -1));
	fd = sceIoOpen(TESTFILE, PSP_O_RDONLY, 0777);
	probe("Default -1, from +10", fd, main + 0x10);
	probe("Default -1, then from +20", fd, main + 0x20);
	sceIoClose(fd);
}

int main(int argc, char *argv[]) {
	removeFiles();
	sceIoRemove(MISSINGFILE);
	createFiles();

	basicTests();
	// nullResultTests() hangs the PSP here: only later SDKs check the pointer.
	closeTests();
	callbackTests();
	dispatchTests();
	interruptTests();
	priorityTests();

	// Can't be set back, so the old-SDK cases go first.
	sceKernelSetCompiledSdkVersion606(0x06060010);
	section("SDK 6.06:");
	nullResultTests();
	closeTests();
	dispatchTests();
	interruptTests();
	priorityTests();

	removeFiles();
	flushschedf();
	return 0;
}
