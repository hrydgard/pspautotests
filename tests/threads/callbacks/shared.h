#include <common.h>

#include <pspthreadman.h>
#include <pspmodulemgr.h>

const static SceUInt NO_TIMEOUT = (SceUInt)-1337;

inline void schedfCallback(SceKernelCallbackInfo &info) {
	schedf("Callback: OK (size=%d,name=%s,thread=%d,callback=%d,common=%08x,notifyCount=%08x,notifyArg=%d)\n", info.size, info.name, info.threadId == 0 ? 0 : 1, info.callback == 0 ? 0 : 1, info.common, info.notifyCount, info.notifyArg);
}

inline void schedfCallback(SceUID cb) {
	if (cb > 0) {
		SceKernelCallbackInfo info;
		info.size = sizeof(info);

		int result = sceKernelReferCallbackStatus(cb, &info);
		if (result == 0) {
			schedfCallback(info);
		} else {
			schedf("Callback: Invalid (%08x)\n", result);
		}
	} else {
		schedf("Callback: Failed (%08x)\n", cb);
	}
}

struct BasicThread {
	BasicThread(const char *name, int prio = 0x60)
		: name_(name) {
		thread_ = sceKernelCreateThread(name, &run, prio, 0x1000, 0, NULL);
	}

	void start() {
		const void *arg[1] = { (void *)this };
		sceKernelStartThread(thread_, sizeof(arg), arg);
		checkpoint("  ** started %s", name_);
	}

	void stop() {
		if (thread_ >= 0) {
			if (sceKernelGetThreadExitStatus(thread_) != 0) {
				sceKernelDelayThread(500);
				sceKernelTerminateDeleteThread(thread_);
				checkpoint("  ** stopped %s", name_);
			} else {
				sceKernelDeleteThread(thread_);
			}
		}
		thread_ = 0;
	}

	static int run(SceSize args, void *argp) {
		BasicThread *o = *(BasicThread **)argp;
		return o->execute();
	}

	virtual int execute() = 0;

	~BasicThread() {
		stop();
	}

	const char *name_;
	SceUID thread_;
};
