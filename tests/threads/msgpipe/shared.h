#include <common.h>

static inline void schedfMsgPipeInfo(SceKernelMppInfo *info) {
	schedf("Msgpipe: OK (size=%d, name=%s, attr=%08x, buffer=%x, free=%x, sending=%d, receiving=%d)\n", info->size, info->name, info->attr, info->bufSize, info->freeSize, info->numSendWaitThreads, info->numReceiveWaitThreads);
}

static inline void schedfMsgPipe(SceUID msgpipe) {
	if (msgpipe >= 0) {
		SceKernelMppInfo info;
		info.size = sizeof(info);

		int result = sceKernelReferMsgPipeStatus(msgpipe, &info);
		if (result == 0) {
			schedfMsgPipeInfo(&info);
		} else {
			schedf("Msgpipe: Invalid (%08x)\n", result);
		}
	} else {
		schedf("Msgpipe: Failed (%08x)\n", msgpipe);
	}
}