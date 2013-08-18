#include <common.h>

#include <pspthreadman.h>
#include <pspmodulemgr.h>

const static SceUInt NO_TIMEOUT = (SceUInt)-1337;

static inline SceUID sceKernelCreateMsgPipe(const char *name, int part, u32 attr, u32 size, void *opt) {
	return sceKernelCreateMsgPipe(name, part, attr, (void *)size, opt);
}

static inline void schedfMsgPipe(SceKernelMppInfo *info) {
	schedf("Msgpipe: OK (size=%d, name=%s, attr=%08x, buffer=%x, free=%x, sending=%d, receiving=%d)\n", info->size, info->name, info->attr, info->bufSize, info->freeSize, info->numSendWaitThreads, info->numReceiveWaitThreads);
}

static inline void schedfMsgPipe(SceUID msgpipe) {
	if (msgpipe >= 0) {
		SceKernelMppInfo info;
		info.size = sizeof(info);

		int result = sceKernelReferMsgPipeStatus(msgpipe, &info);
		if (result == 0) {
			schedfMsgPipe(&info);
		} else {
			schedf("Msgpipe: Invalid (%08x)\n", result);
		}
	} else {
		schedf("Msgpipe: Failed (%08x)\n", msgpipe);
	}
}

template <void (*Wait)(const char *name, SceUID uid, SceUInt timeout)>
struct KernelObjectWaitThread {
	KernelObjectWaitThread(const char *name, SceUID uid, SceUInt timeout, int prio = 0x60)
		: name_(name), object_(uid), timeout_(timeout) {
		thread_ = sceKernelCreateThread(name, &run, prio, 0x1000, 0, NULL);
		const void *arg[1] = { (void *)this };
		sceKernelStartThread(thread_, sizeof(arg), arg);
		sceKernelDelayThread(1000);
		checkpoint("  ** started %s", name_);
	}

	static int run(SceSize args, void *argp) {
		KernelObjectWaitThread<Wait> **o = (KernelObjectWaitThread<Wait> **) argp;
		return (*o)->wait();
	}

	int wait() {
		Wait(name_, object_, timeout_);
		return 0;
	}

	void stop() {
		if (thread_ >= 0) {
			if (sceKernelGetThreadExitStatus(thread_) != 0) {
				sceKernelDelayThread(1000);
				sceKernelTerminateDeleteThread(thread_);
				checkpoint("  ** stopped %s", name_);
			} else {
				sceKernelDeleteThread(thread_);
			}
		}
		thread_ = 0;
	}

	~KernelObjectWaitThread() {
		stop();
	}

	const char *name_;
	SceUID thread_;
	SceUID object_;
	SceUInt timeout_;
};

void MsgPipeReceiveDoWait(const char *name, SceUID uid, SceUInt timeout) {
	char msg[256];
	int received = 0x1337;
	if (timeout == NO_TIMEOUT) {
		int result = sceKernelReceiveMsgPipe(uid, msg, sizeof(msg), 0, &received, NULL);
		checkpoint("  ** %s got result: %08x, received = %08x", name, result, received);
	} else {
		int result = sceKernelReceiveMsgPipe(uid, msg, sizeof(msg), 0, &received, &timeout);
		checkpoint("  ** %s got result: %08x, received = %08x, timeout = %dms remaining", name, result, received, timeout / 1000);
	}
}

void MsgPipeSendDoWait(const char *name, SceUID uid, SceUInt timeout) {
	char msg[256];
	int sent = 0x1337;
	if (timeout == NO_TIMEOUT) {
		int result = sceKernelSendMsgPipe(uid, msg, sizeof(msg), 0, &sent, NULL);
		checkpoint("  ** %s got result: %08x, sent = %08x", name, result, sent);
	} else {
		int result = sceKernelSendMsgPipe(uid, msg, sizeof(msg), 0, &sent, &timeout);
		checkpoint("  ** %s got result: %08x, sent = %08x, timeout = %dms remaining", name, result, sent, timeout / 1000);
	}
}

typedef KernelObjectWaitThread<MsgPipeReceiveDoWait> MsgPipeReceiveWaitThread;
typedef KernelObjectWaitThread<MsgPipeSendDoWait> MsgPipeSendWaitThread;
