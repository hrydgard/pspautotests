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
