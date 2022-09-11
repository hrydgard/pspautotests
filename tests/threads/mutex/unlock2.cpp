extern "C" {
#include "shared.h"
}

struct ThreadArg {
	SceUID id;
	int bits;
};

struct KernelThread {
	KernelThread(const char *name, SceKernelThreadEntry entry, int priority, int stackSize = 0x1000, SceUInt attr = 0, SceKernelThreadOptParam *opt = NULL) {
		id_ = sceKernelCreateThread(name, entry, priority, stackSize, attr, opt);
	}
	~KernelThread() {
		if (id_ >= 0) {
			sceKernelDeleteThread(id_);
		}
	}

	template <typename T>
	void Start(T &arg) {
		sceKernelStartThread(id_, sizeof(arg), &arg);
	}

	SceUID id_;
};

static int threadsRun = 0;

static int waitLock(SceSize argSize, void *argPointer) {
	ThreadArg *arg = (ThreadArg *)argPointer;
	SceUInt timeout = 1000;
	schedulingResult = sceKernelLockMutex(arg->id, 1, &timeout);
	threadsRun |= arg->bits;
	sceKernelUnlockMutex(arg->id, 1);
	return 0;
}

extern "C" int main(int argc, char **argv) {
	if (sceKernelGetThreadCurrentPriority() != 0x20) {
		printf("Test failure: main thread expected to be priority=0x20\n");
		return 1;
	}

	SceUID id = sceKernelCreateMutex("test", 0, 1, NULL);
	KernelThread worseThread("worse", &waitLock, 0x30, 0x1000, 0, NULL);
	KernelThread sameThread("same", &waitLock, 0x20, 0x1000, 0, NULL);
	KernelThread betterThread("better", &waitLock, 0x10, 0x1000, 0, NULL);

	if (id < 0 || worseThread.id_ < 0 || sameThread.id_ < 0 || betterThread.id_ < 0) {
		printf("Test failure: could not create test objects\n");
		return 2;
	}

	ThreadArg worseArg = { id, 1 };
	worseThread.Start(worseArg);
	ThreadArg sameArg = { id, 2 };
	sameThread.Start(sameArg);
	ThreadArg betterArg = { id, 4 };
	betterThread.Start(betterArg);

	if (threadsRun != 0) {
		printf("Test failure: threads not blocked by lock\n");
		sceKernelDeleteMutex(id);
		return 3;
	}

	// Uncomment this line to log timestamps of the each checkpoint.
	//CHECKPOINT_ENABLE_TIME = 1;

	checkpointNext("Unlock test");
	sceKernelUnlockMutex(id, 1);
	int immediately = threadsRun;
	checkpoint("  Unlocked, ran: %x", immediately);
	sceKernelDelayThread(200);
	checkpoint("  Delayed, ran: %x", threadsRun);

	sceKernelDeleteMutex(id);

	return 0;
}