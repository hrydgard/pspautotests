#include <common.h>

#include <pspthreadman.h>
#include <pspmodulemgr.h>

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