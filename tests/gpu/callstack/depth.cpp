#include <common.h>
#include <pspge.h>
#include <pspthreadman.h>
#include <psputils.h>

#include "../commands/commands.h"

// How deeply the GE nests CALLs. Each level writes one light color register before its CALL and
// one after, and the main list does the same, so reading them back shows which commands ran.

static u32 __attribute__((aligned(16))) list[4096];

static const int LEVEL_SIZE = 256;

static u32 *emit(u32 *p, u32 cmd, u32 param) {
	*p++ = (cmd << 24) | (param & 0x00FFFFFF);
	return p;
}

static u32 *emitCall(u32 *p, const u32 *target) {
	p = emit(p, GE_CMD_BASE, ((u32)target >> 8) & 0x000F0000);
	return emit(p, GE_CMD_CALL, (u32)target & 0x00FFFFFF);
}

static u32 *emitSignalCall(u32 *p, const u32 *target) {
	p = emit(p, GE_CMD_SIGNAL, (PSP_GE_SIGNAL_CALL << 16) | ((u32)target >> 16));
	return emit(p, GE_CMD_END, (u32)target & 0x0000FFFF);
}

static u32 *emitSignalRet(u32 *p) {
	p = emit(p, GE_CMD_SIGNAL, PSP_GE_SIGNAL_RET << 16);
	return emit(p, GE_CMD_END, 0);
}

static u32 *section(int i) {
	return &list[LEVEL_SIZE * i];
}

static SceGeStack stacks[8];

static void runAndPrint(const char *title, int markers) {
	sceKernelDcacheWritebackInvalidateAll();

	PspGeListArgs args;
	args.size = sizeof(args);
	args.context = NULL;
	args.numStacks = 8;
	args.stacks = stacks;
	int listid = sceGeListEnQueue(list, NULL, -1, &args);
	int result = sceGeListSync(listid, 0);
	checkpoint("%s: enqueue=%s sync=%08x", title, listid >= 0 ? "OK" : "FAIL", result);
	for (int i = 0; i < markers; ++i) {
		checkpoint("  Marker %d: %06x", i, sceGeGetCmd(GE_CMD_LAC0 + i) & 0x00FFFFFF);
	}
}

static u32 *clearMarkers() {
	memset(list, 0, sizeof(list));
	u32 *p = list;
	for (int i = 0; i < 12; ++i) {
		p = emit(p, GE_CMD_LAC0 + i, 0);
	}
	return p;
}

// main -> CALL L1 -> CALL L2 -> SIGNAL CALL S -> CALL T, and after S returns, L2 -> CALL U.
static void testSignalCallAtDepth2() {
	u32 *p = clearMarkers();
	p = emitCall(p, section(1));
	p = emit(p, GE_CMD_LAC0 + 0, 0x100);
	p = emit(p, GE_CMD_FINISH, 0);
	p = emit(p, GE_CMD_END, 0);

	p = section(1);
	p = emitCall(p, section(2));
	p = emit(p, GE_CMD_LAC0 + 1, 0x110);
	p = emit(p, GE_CMD_RET, 0);

	p = section(2);
	p = emitSignalCall(p, section(3));
	p = emit(p, GE_CMD_LAC0 + 2, 0x120);
	p = emitCall(p, section(5));
	p = emit(p, GE_CMD_LAC0 + 7, 0x121);
	p = emit(p, GE_CMD_RET, 0);

	p = section(3);
	p = emit(p, GE_CMD_LAC0 + 3, 0x130);
	p = emitCall(p, section(4));
	p = emit(p, GE_CMD_LAC0 + 4, 0x131);
	p = emitSignalRet(p);

	p = section(4);
	p = emit(p, GE_CMD_LAC0 + 5, 0x140);
	p = emit(p, GE_CMD_RET, 0);

	p = section(5);
	p = emit(p, GE_CMD_LAC0 + 6, 0x150);
	p = emit(p, GE_CMD_RET, 0);

	runAndPrint("Signal call at depth 2, then CALL", 8);
}

// main -> CALL L1 -> SIGNAL CALL S -> CALL A -> CALL B.
static void testCallsInsideSignalCall() {
	u32 *p = clearMarkers();
	p = emitCall(p, section(1));
	p = emit(p, GE_CMD_LAC0 + 0, 0x100);
	p = emit(p, GE_CMD_FINISH, 0);
	p = emit(p, GE_CMD_END, 0);

	p = section(1);
	p = emitSignalCall(p, section(2));
	p = emit(p, GE_CMD_LAC0 + 1, 0x110);
	p = emit(p, GE_CMD_RET, 0);

	p = section(2);
	p = emitCall(p, section(3));
	p = emit(p, GE_CMD_LAC0 + 2, 0x120);
	p = emitSignalRet(p);

	p = section(3);
	p = emitCall(p, section(4));
	p = emit(p, GE_CMD_LAC0 + 3, 0x130);
	p = emit(p, GE_CMD_RET, 0);

	p = section(4);
	p = emit(p, GE_CMD_LAC0 + 4, 0x140);
	p = emit(p, GE_CMD_RET, 0);

	runAndPrint("Signal call at depth 1, then two CALLs", 5);
}

static void testDepth(const char *title, int depth) {
	memset(list, 0, sizeof(list));

	u32 *p = list;
	for (int i = 0; i < 12; ++i) {
		p = emit(p, GE_CMD_LAC0 + i, 0);
	}
	p = emit(p, GE_CMD_LAC0, 0x100);
	p = emitCall(p, &list[LEVEL_SIZE]);
	p = emit(p, GE_CMD_LAC0 + 1, 0x101);
	p = emit(p, GE_CMD_FINISH, 0);
	p = emit(p, GE_CMD_END, 0);

	for (int level = 1; level <= depth; ++level) {
		p = &list[LEVEL_SIZE * level];
		p = emit(p, GE_CMD_LAC0 + level * 2, 0x100 + level * 0x10);
		if (level < depth) {
			p = emitCall(p, &list[LEVEL_SIZE * (level + 1)]);
		}
		p = emit(p, GE_CMD_LAC0 + level * 2 + 1, 0x101 + level * 0x10);
		p = emit(p, GE_CMD_RET, 0);
	}
	sceKernelDcacheWritebackInvalidateAll();

	int listid = sceGeListEnQueue(list, NULL, -1, NULL);
	int result = sceGeListSync(listid, 0);
	checkpoint("%s: enqueue=%s sync=%08x", title, listid >= 0 ? "OK" : "FAIL", result);
	for (int level = 0; level <= 4; ++level) {
		checkpoint("  Level %d: before=%06x after=%06x", level, sceGeGetCmd(GE_CMD_LAC0 + level * 2) & 0x00FFFFFF, sceGeGetCmd(GE_CMD_LAC0 + level * 2 + 1) & 0x00FFFFFF);
	}
}

extern "C" int main(int argc, char *argv[]) {
	testDepth("Depth 1", 1);
	testDepth("Depth 2", 2);
	testDepth("Depth 3", 3);
	testDepth("Depth 4", 4);
	testDepth("Depth 2 again", 2);
	testSignalCallAtDepth2();
	testCallsInsideSignalCall();
	return 0;
}
