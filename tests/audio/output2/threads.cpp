#include <common.h>
#include <pspaudio.h>
#include <pspthreadman.h>

enum MsgType {
	MSG_RESERVE,
	MSG_RELEASE,
	MSG_QUIT,
};

struct Msg {
	Msg(MsgType ty, int i1 = 0) {
		memset(&header, 0, sizeof(header));
		type = ty;
		int1 = i1;
	}

	SceKernelMsgPacket header;
	MsgType type;
	int int1;
};

struct ThreadInfo {
	int thnum;
	SceUID mbox;
};

int threadFunc(SceSize args, void *argp) {
	ThreadInfo *info = (ThreadInfo *)argp;
	Msg *msg;

	while (sceKernelReceiveMbx(info->mbox, (void **)&msg, NULL) == 0) {
		switch (msg->type) {
		case MSG_QUIT:
			info->mbox = 0;
			break;

		case MSG_RESERVE:
			checkpoint("  [%d] Reserve: %08x", info->thnum, sceAudioOutput2Reserve(msg->int1));
			break;

		case MSG_RELEASE:
			checkpoint("  [%d] Release: %08x", info->thnum, sceAudioOutput2Release());
			break;

		default:
			checkpoint("  [%d] Unknown msg type %d", info->thnum, msg->type);
			break;
		}

		delete msg;
	}

	return 0;
}

extern "C" int main(int argc, char *argv[]) {
	SceUID thread1 = sceKernelCreateThread("thread1", &threadFunc, 0x1a, 0x1000, 0, NULL);
	SceUID thread2 = sceKernelCreateThread("thread2", &threadFunc, 0x1a, 0x1000, 0, NULL);

	ThreadInfo info1 = { 1 };
	info1.mbox = sceKernelCreateMbx("mbx1", 0, NULL);
	ThreadInfo info2 = { 2 };
	info2.mbox = sceKernelCreateMbx("mbx2", 0, NULL);

	sceKernelStartThread(thread1, sizeof(ThreadInfo), &info1);
	sceKernelStartThread(thread2, sizeof(ThreadInfo), &info2);

	sceKernelSendMbx(info1.mbox, new Msg(MSG_RESERVE, 0x100));
	sceKernelSendMbx(info2.mbox, new Msg(MSG_RESERVE, 0x800));

	sceKernelSendMbx(info1.mbox, new Msg(MSG_RELEASE));
	sceKernelSendMbx(info2.mbox, new Msg(MSG_RELEASE));

	sceKernelSendMbx(info1.mbox, new Msg(MSG_RESERVE, 0x100));
	sceKernelSendMbx(info2.mbox, new Msg(MSG_RELEASE));
	sceKernelSendMbx(info2.mbox, new Msg(MSG_RESERVE, 0x800));
	sceKernelSendMbx(info1.mbox, new Msg(MSG_RELEASE));

	sceKernelSendMbx(info1.mbox, new Msg(MSG_QUIT));
	sceKernelSendMbx(info2.mbox, new Msg(MSG_QUIT));

	return 0;
}