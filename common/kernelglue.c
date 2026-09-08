// Support code that only a kernel-mode test module needs. Nothing here is compiled into the
// normal user-mode libcommon.
//
// Kernel builds use USE_KERNEL_LIBS, because the stock crt0_prx references __libcglue_init and
// so drags the whole of libcglue in - and libcglue imports sceNetInet, sceUtility and the ForUser
// IO libraries, none of which a kernel module is allowed to import. It fails to load with
// 8002013C, library not found, and trimming LIBS can't help because the reference comes from the
// startup object.
//
// USE_KERNEL_LIBS brings -nostdlib, which means no startup object and none of newlib's support
// hooks. That's what this file puts back: an entry point, a heap, and the small set of stubs
// newlib wants. It's deliberately minimal - enough for vsnprintf and sceIo, not a libc port.

#ifdef COMMON_KERNEL

#include <errno.h>
#include <stdlib.h>
#include <sys/lock.h>
#include <sys/stat.h>

#include <pspkernel.h>
#include <pspiofilemgr.h>

extern int main(int argc, char *argv[]);

// A fixed heap, since there's no sceKernelAllocPartitionMemory-backed sbrk here and the kernel
// partition has very little to spare anyway. Grow it if a kernel test ever needs more.
#define KERNEL_HEAP_SIZE (4 * 1024)
static char kernelHeap[KERNEL_HEAP_SIZE] __attribute__((aligned(16)));
static unsigned int kernelHeapUsed = 0;

void *_sbrk(int incr) {
	if (incr < 0) {
		// Never handing memory back is fine for a test that runs once and exits.
		return kernelHeap + kernelHeapUsed;
	}
	if (kernelHeapUsed + (unsigned int)incr > KERNEL_HEAP_SIZE) {
		errno = ENOMEM;
		return (void *)-1;
	}
	char *p = kernelHeap + kernelHeapUsed;
	kernelHeapUsed += (unsigned int)incr;
	return p;
}

// Output goes through sceIo directly (see common.c), so newlib never needs a real file
// descriptor. These exist only to satisfy the linker.
int _close(int fd) { return -1; }
int _lseek(int fd, int offset, int whence) { return -1; }
int _read(int fd, char *buf, int len) { return -1; }
int _write(int fd, const char *buf, int len) { return len; }
int _getpid(void) { return 1; }
int _kill(int pid, int sig) { return -1; }

int _fstat(int fd, struct stat *st) {
	st->st_mode = S_IFCHR;
	return 0;
}

void _exit(int code) {
	sceKernelExitDeleteThread(code);
	// Not reached, but _exit must not return.
	for (;;) {
	}
}

// Newlib's retargetable locks. A test module doesn't have two threads inside newlib at once, so
// these can be empty - which is also what keeps libcglue (and its imports) out of the link.
struct __lock {
	int dummy;
};
struct __lock __lock___sfp_recursive_mutex;
struct __lock __lock___atexit_recursive_mutex;
struct __lock __lock___malloc_recursive_mutex;

void __retarget_lock_init_recursive(_LOCK_T *lock) {}
void __retarget_lock_close_recursive(_LOCK_T lock) {}
void __retarget_lock_acquire_recursive(_LOCK_T lock) {}
void __retarget_lock_release_recursive(_LOCK_T lock) {}

// With no crt0 there's no module_start, and prxexports.o wants one. Run the test the same way
// the user-mode startup would.
int module_start(SceSize args, void *argp) {
	char *argv[1] = { (char *)"kernel_test" };
	return main(1, argv);
}

int module_stop(SceSize args, void *argp) {
	return 0;
}

#endif  // COMMON_KERNEL
