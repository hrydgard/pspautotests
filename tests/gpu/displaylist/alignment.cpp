#include <common.h>

#include <stdlib.h>
#include <stdio.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspthreadman.h>
#include <pspkernel.h>

extern "C" {
#include "sysmem-imports.h"
#include "../commands/commands.h"
}

u32 dlistReset[] = {
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000000,
	// Try to make sure we always finish.
	(GE_CMD_FINISH << 24) | 0x000000,
	(GE_CMD_END << 24) | 0x000000,
};

u32 dlist1[] = {
	// Add a NOP that looks like an instruction when read at an offset.
	(GE_CMD_NOP << 24) | (GE_CMD_AMBIENTCOLOR << 16) | (GE_CMD_AMBIENTCOLOR << 8) | GE_CMD_AMBIENTCOLOR,
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000001,
	// Try to make sure we always finish.
	(GE_CMD_FINISH << 24) | (GE_CMD_FINISH << 16) | (GE_CMD_FINISH << 8) | GE_CMD_FINISH,
	(GE_CMD_END << 24) | (GE_CMD_END < 16) | (GE_CMD_END << 8) | GE_CMD_END,
	(GE_CMD_NOP << 24),
};

u32 dlist2[] = {
	(GE_CMD_BASE << 24) | 0x000000,
	(GE_CMD_ORIGIN << 24),
	// Didn't jump at all?
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000000,
	// This is overwritten by the actual jump offset.
	(GE_CMD_JUMP << 24) | 0x00001D,
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000009,
	(GE_CMD_FINISH << 24) | 0x000000,
	(GE_CMD_END << 24) | 0x000000,

	// Add a NOP that looks like an instruction when read at an offset.
	(GE_CMD_NOP << 24) | (GE_CMD_AMBIENTCOLOR << 16) | (GE_CMD_AMBIENTCOLOR << 8) | GE_CMD_AMBIENTCOLOR,
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000002,
	// Try to make sure we always finish.
	(GE_CMD_FINISH << 24) | (GE_CMD_FINISH << 16) | (GE_CMD_FINISH << 8) | GE_CMD_FINISH,
	(GE_CMD_END << 24) | (GE_CMD_END < 16) | (GE_CMD_END << 8) | GE_CMD_END,
	(GE_CMD_NOP << 24),
};

u32 dlist3[] = {
	(GE_CMD_BOUNDINGBOX << 24) | 0x000000,
	(GE_CMD_BASE << 24) | 0x000000,
	(GE_CMD_ORIGIN << 24),
	// Didn't jump at all?
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000000,
	// This is overwritten by the actual jump offset.
	(GE_CMD_BJUMP << 24) | (0x00001D),
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000009,
	(GE_CMD_FINISH << 24) | 0x000000,
	(GE_CMD_END << 24) | 0x000000,

	// Add a NOP that looks like an instruction when read at an offset.
	(GE_CMD_NOP << 24) | (GE_CMD_AMBIENTCOLOR << 16) | (GE_CMD_AMBIENTCOLOR << 8) | GE_CMD_AMBIENTCOLOR,
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000003,
	// Try to make sure we always finish.
	(GE_CMD_FINISH << 24) | (GE_CMD_FINISH << 16) | (GE_CMD_FINISH << 8) | GE_CMD_FINISH,
	(GE_CMD_END << 24) | (GE_CMD_END < 16) | (GE_CMD_END << 8) | GE_CMD_END,
	(GE_CMD_NOP << 24),
};

u32 dlist4[] = {
	(GE_CMD_BASE << 24) | 0x000000,
	(GE_CMD_ORIGIN << 24),
	// Didn't jump at all?
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000000,
	// This is overwritten by the actual jump offset.
	(GE_CMD_CALL << 24) | 0x00001D,
	(GE_CMD_FINISH << 24) | 0x000000,
	(GE_CMD_END << 24) | 0x000000,

	// Add a NOP that looks like an instruction when read at an offset.
	(GE_CMD_NOP << 24) | (GE_CMD_AMBIENTCOLOR << 16) | (GE_CMD_AMBIENTCOLOR << 8) | GE_CMD_AMBIENTCOLOR,
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000004,
	// Try to make sure we always finish.
	(GE_CMD_RET << 24) | (GE_CMD_RET < 16) | (GE_CMD_RET << 8) | GE_CMD_RET,
	(GE_CMD_NOP << 24),
};

u32 dlist5[] = {
	(GE_CMD_BASE << 24) | 0x000000,
	(GE_CMD_ORIGIN << 24),
	// Didn't jump at all?
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000000,
	// These two are overwritten by the actual jump offset.
	(GE_CMD_SIGNAL << 24) | (0x10 << 16) | 0x0000,
	(GE_CMD_END << 24) | 0x000000,
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000009,
	(GE_CMD_FINISH << 24) | 0x000000,
	(GE_CMD_END << 24) | 0x000000,

	// Add a NOP that looks like an instruction when read at an offset.
	(GE_CMD_NOP << 24) | (GE_CMD_AMBIENTCOLOR << 16) | (GE_CMD_AMBIENTCOLOR << 8) | GE_CMD_AMBIENTCOLOR,
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000005,
	// Try to make sure we always finish.
	(GE_CMD_FINISH << 24) | (GE_CMD_FINISH << 16) | (GE_CMD_FINISH << 8) | GE_CMD_FINISH,
	(GE_CMD_END << 24) | (GE_CMD_END < 16) | (GE_CMD_END << 8) | GE_CMD_END,
	(GE_CMD_NOP << 24),
};

u32 dlist6[] = {
	(GE_CMD_BASE << 24) | 0x000000,
	(GE_CMD_ORIGIN << 24),
	// Didn't jump at all?
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000000,
	// These two are overwritten by the actual jump offset.
	(GE_CMD_SIGNAL << 24) | (0x11 << 16) | 0x0000,
	(GE_CMD_END << 24) | 0x000000,
	(GE_CMD_FINISH << 24) | 0x000000,
	(GE_CMD_END << 24) | 0x000000,

	// Add a NOP that looks like an instruction when read at an offset.
	(GE_CMD_NOP << 24) | (GE_CMD_AMBIENTCOLOR << 16) | (GE_CMD_AMBIENTCOLOR << 8) | GE_CMD_AMBIENTCOLOR,
	(GE_CMD_AMBIENTCOLOR << 24) | 0x000006,
	// This is the signal return.
	(GE_CMD_SIGNAL << 24) | (0x12 << 16) | 0x0000,
	(GE_CMD_END << 24) | 0x000000,
	// Try to make sure we always finish, just to be safe.
	(GE_CMD_FINISH << 24) | (GE_CMD_FINISH << 16) | (GE_CMD_FINISH << 8) | GE_CMD_FINISH,
	(GE_CMD_END << 24) | (GE_CMD_END < 16) | (GE_CMD_END << 8) | GE_CMD_END,
	(GE_CMD_NOP << 24),
};

u32 runList(void *list, void *listend, bool head = false) {
	int listid = head ? sceGeListEnQueueHead(list, list, -1, NULL) : sceGeListEnQueue(list, list, -1, NULL);
	if (listid < 0) {
		return listid;
	}
	sceGeListUpdateStallAddr(listid, listend);
	if (head) {
		sceGeContinue();
	}
	sceKernelDelayThread(5000);
	sceGeBreak(1, NULL);

	return sceGeGetCmd(GE_CMD_AMBIENTCOLOR);
}

u32 runListAndReset(void *list, void *listend, bool head = false) {
	u32 result = runList(list, listend, head);
	runList(dlistReset, dlistReset + 100);
	return result;
}

u32 runJumpList(int offset) {
	dlist2[3] = (GE_CMD_JUMP << 24) | (0x000018 + offset);
	sceKernelDcacheWritebackInvalidateRange(dlist2, sizeof(dlist2));

	return runList(dlist2, dlist2 + 100);
}

u32 runBJumpList(int offset) {
	dlist3[4] = (GE_CMD_BJUMP << 24) | (0x000018 + offset);
	sceKernelDcacheWritebackInvalidateRange(dlist3, sizeof(dlist3));

	return runList(dlist3, dlist3 + 100);
}

u32 runCallList(int offset) {
	dlist4[3] = (GE_CMD_CALL << 24) | (0x000014 + offset);
	sceKernelDcacheWritebackInvalidateRange(dlist4, sizeof(dlist4));

	return runList(dlist4, dlist4 + 100);
}

u32 runSignalJumpList(int offset) {
	u32 target = (intptr_t)dlist5 + 0x20 + offset;
	dlist5[3] = (GE_CMD_SIGNAL << 24) | (0x10 << 16) | (target >> 16);
	dlist5[4] = (GE_CMD_END << 24) | (target & 0xFFFF);
	sceKernelDcacheWritebackInvalidateRange(dlist5, sizeof(dlist5));

	return runList(dlist5, dlist5 + 100);
}

u32 runSignalCallList(int offset) {
	u32 target = (intptr_t)dlist6 + 0x1C + offset;
	dlist6[3] = (GE_CMD_SIGNAL << 24) | (0x10 << 16) | (target >> 16);
	dlist6[4] = (GE_CMD_END << 24) | (target & 0xFFFF);
	sceKernelDcacheWritebackInvalidateRange(dlist6, sizeof(dlist6));

	return runList(dlist6, dlist6 + 100);
}

extern "C" int main(int argc, char *argv[]) {
	checkpointNext("Enqueue");
	checkpoint("  Aligned: %x", runListAndReset(dlist1, dlist1 + 100));
	checkpoint("  Offset + 1: %x", runListAndReset((u8 *)dlist1 + 1, dlist1 + 100));
	checkpoint("  Offset + 2: %x", runListAndReset((u8 *)dlist1 + 2, dlist1 + 100));
	checkpoint("  Offset + 3: %x", runListAndReset((u8 *)dlist1 + 3, dlist1 + 100));
	checkpoint("  Offset + 4: %x", runListAndReset((u8 *)dlist1 + 4, dlist1 + 100));
	checkpoint("  Offset + 8: %x", runListAndReset((u8 *)dlist1 + 8, dlist1 + 100));

	checkpointNext("EnqueueHead");
	checkpoint("  Aligned: %x", runListAndReset(dlist1, dlist1 + 100, true));
	checkpoint("  Offset + 1: %x", runListAndReset((u8 *)dlist1 + 1, dlist1 + 100, true));
	checkpoint("  Offset + 2: %x", runListAndReset((u8 *)dlist1 + 2, dlist1 + 100, true));
	checkpoint("  Offset + 3: %x", runListAndReset((u8 *)dlist1 + 3, dlist1 + 100, true));
	checkpoint("  Offset + 4: %x", runListAndReset((u8 *)dlist1 + 4, dlist1 + 100, true));
	checkpoint("  Offset + 8: %x", runListAndReset((u8 *)dlist1 + 8, dlist1 + 100, true));

	checkpointNext("Jump");
	checkpoint("  Aligned: %x", runJumpList(0));
	checkpoint("  Offset + 1: %x", runJumpList(1));
	checkpoint("  Offset + 2: %x", runJumpList(2));
	checkpoint("  Offset + 3: %x", runJumpList(3));
	checkpoint("  Offset + 4: %x", runJumpList(4));
	checkpoint("  Offset + 8: %x", runJumpList(8));

	checkpointNext("BJump");
	checkpoint("  Aligned: %x", runBJumpList(0));
	checkpoint("  Offset + 1: %x", runBJumpList(1));
	checkpoint("  Offset + 2: %x", runBJumpList(2));
	checkpoint("  Offset + 3: %x", runBJumpList(3));
	checkpoint("  Offset + 4: %x", runBJumpList(4));
	checkpoint("  Offset + 8: %x", runBJumpList(8));

	checkpointNext("Call");
	checkpoint("  Aligned: %x", runCallList(0));
	checkpoint("  Offset + 1: %x", runCallList(1));
	checkpoint("  Offset + 2: %x", runCallList(2));
	checkpoint("  Offset + 3: %x", runCallList(3));
	checkpoint("  Offset + 4: %x", runCallList(4));
	checkpoint("  Offset + 8: %x", runCallList(8));

	checkpointNext("Signal Jump");
	checkpoint("  Aligned: %x", runSignalJumpList(0));
	checkpoint("  Offset + 1: %x", runSignalJumpList(1));
	checkpoint("  Offset + 2: %x", runSignalJumpList(2));
	checkpoint("  Offset + 3: %x", runSignalJumpList(3));
	checkpoint("  Offset + 4: %x", runSignalJumpList(4));
	checkpoint("  Offset + 8: %x", runSignalJumpList(8));

	checkpointNext("Signal Call");
	checkpoint("  Aligned: %x", runSignalCallList(0));
	checkpoint("  Offset + 1: %x", runSignalCallList(1));
	checkpoint("  Offset + 2: %x", runSignalCallList(2));
	checkpoint("  Offset + 3: %x", runSignalCallList(3));
	checkpoint("  Offset + 4: %x", runSignalCallList(4));
	checkpoint("  Offset + 8: %x", runSignalCallList(8));

	return 0;
}