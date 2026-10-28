// sceNetInetGetErrno and GetPspError across threads: whether each thread has its own errno, what a thread that has
// never made a call reads, and what calls do on a thread with too little stack. In the firmware,
// pspnet.prx keeps a record per thread ID, found or made on a call, holding errno, and every
// sceNetInet call first checks that 0xFF0 bytes of stack are free.

#include "inet.h"
#include <pspthreadman.h>

static volatile int first, firstPspError, afterCall, afterOwnFailure, smallStackGetErrno, smallStackPspError, smallStackSocket;

static int otherThread(SceSize args, void *argp) {
	first = sceNetInetGetErrno();
	firstPspError = sceNetInetGetPspError();
	int s = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	afterCall = sceNetInetGetErrno();
	sceNetInetClose(s);
	sceNetInetSocket(0, INET_SOCK_STREAM, 0);
	afterOwnFailure = sceNetInetGetErrno();
	return 0;
}

static int smallStackThread(SceSize args, void *argp) {
	smallStackGetErrno = sceNetInetGetErrno();
	smallStackPspError = sceNetInetGetPspError();
	smallStackSocket = sceNetInetSocket(INET_AF_INET, INET_SOCK_DGRAM, 0);
	if (smallStackSocket >= 0) {
		sceNetInetClose(smallStackSocket);
	}
	return 0;
}

static void run(const char *name, SceKernelThreadEntry entry, int stack) {
	SceUID thid = sceKernelCreateThread(name, entry, 0x18, stack, PSP_THREAD_ATTR_USER, NULL);
	sceKernelStartThread(thid, 0, NULL);
	sceKernelWaitThreadEnd(thid, NULL);
	sceKernelDeleteThread(thid);
}

int main(int argc, char *argv[]) {
	if (!inetStartup()) {
		return 1;
	}

	sceNetInetClose(200);
	printf("Main thread after a failure: %d, GetPspError %08x\n", sceNetInetGetErrno(), sceNetInetGetPspError());
	run("other", &otherThread, 0x10000);
	printf("Another thread, before any call: %08x, GetPspError %08x\n", first, firstPspError);
	printf("  After a successful call: %d\n", afterCall);
	printf("  After a failure of its own: %d\n", afterOwnFailure);
	printf("Main thread afterwards: %d\n", sceNetInetGetErrno());

	run("small", &smallStackThread, 0x1000);
	printf("4 KB stack: GetErrno %08x, GetPspError %08x, socket %08x\n", smallStackGetErrno, smallStackPspError, smallStackSocket);

	inetShutdown();
	return 0;
}
