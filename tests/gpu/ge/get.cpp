#include <common.h>
#include <pspge.h>
#include <pspthreadman.h>
#include <psputils.h>

#include "../commands/commands.h"

#include "sysmem-imports.h"

struct SceGeStack {
	int v[8];
};
extern "C" int sceGeGetStack(int level, SceGeStack *stack);

static u32 __attribute__((aligned(16))) list[262144];

void testGetStack(const char *title, int level, bool useStack = true) {
	SceGeStack stack;
	memset(&stack, 0xCC, sizeof(stack));
	checkpoint("  %s: %08x", title, sceGeGetStack(level, useStack ? &stack : NULL));
	// I'm not sure what the others are, or if they're important.
	// So far, no games have been seen accessing this, so let's just make sure the values we know are right.
	checkpoint("     * 0: %08x", 0, stack.v[0]);
	checkpoint("     * 1: %08x", 1, stack.v[1]);
	checkpoint("     * 2: %08x", 2, stack.v[2]);
	checkpoint("     * 7: %08x", 7, stack.v[7]);
}

extern "C" void signalFunc(int id, void *arg) {
	int listid = *(int *)arg;
	testGetStack("-1", -1);
	testGetStack("0", 0);
	testGetStack("255", 255);
	testGetStack("NULL stack", 0, false);
}

void runGetStackTests() {
	int listid;

	memset(list, 0, sizeof(list));
	//list[0] = (GE_CMD_BASE << 24) | 0x09FFFF;
	list[0] = (GE_CMD_BASE << 24) | ((((int)list) & 0xFF000000) >> 8);
	list[1] = (GE_CMD_VADDR << 24) | 0xAA1337;
	list[2] = (GE_CMD_IADDR << 24) | 0xBB1337;
	list[3] = (GE_CMD_OFFSETADDR << 24) | 0;
	list[3] = (GE_CMD_OFFSETADDR << 24) | 0xCC1337;
	//list[4] = (GE_CMD_CALL << 24) | ((u32)(&list[200]) & 0x00FFFFFF);
	list[10] = (GE_CMD_SIGNAL << 24) | (PSP_GE_SIGNAL_CALL << 16) | ((u32)(&list[1000]) >> 16);
	list[11] = (GE_CMD_END << 24) | ((u32)(&list[1000]) & 0x0000FFFF);
	list[100] = GE_CMD_FINISH << 24;
	list[101] = GE_CMD_END << 24;
	list[200] = (GE_CMD_BASE << 24) | 0x09FFFF;
	list[201] = (GE_CMD_OFFSETADDR << 24) | 0xCC1337;
	list[202] = GE_CMD_RET << 24;
	list[1000] = (GE_CMD_BASE << 24) | 0x0AEEEE;
	list[1010] = (GE_CMD_SIGNAL << 24) | (PSP_GE_SIGNAL_HANDLER_SUSPEND << 16);
	list[1011] = GE_CMD_END << 24;
	list[1020] = (GE_CMD_SIGNAL << 24) | (PSP_GE_SIGNAL_RET << 16);
	list[1021] = GE_CMD_END << 24;
	list[1100] = GE_CMD_FINISH << 24;
	list[1101] = GE_CMD_END << 24;
	sceKernelDcacheWritebackRange(list, sizeof(list));

	checkpointNext("sceGeGetStack - without list:");
	testGetStack("-1", -1);
	testGetStack("0", 0);
	testGetStack("255", 255);
	testGetStack("NULL stack", 0, false);

	PspGeCallbackData cb;
	cb.signal_func = signalFunc;
	cb.signal_arg = &listid;
	cb.finish_func = NULL;
	int cbID = sceGeSetCallback(&cb);

	checkpointNext("sceGeGetStack - within list:");
	listid = sceGeListEnQueue(list, list, cbID, NULL);
	sceGeListUpdateStallAddr(listid, &list[2000]);
	sceKernelDelayThread(10000);
	sceGeUnsetCallback(cbID);
	checkpoint("  Base address: %08x", sceGeGetCmd(GE_CMD_BASE));
}

extern "C" int main(int argc, char *argv[]) {
	runGetStackTests();
	return 0;
}