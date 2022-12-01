#include <common.h>
#include <malloc.h>
#include <pspge.h>
#include <psputils.h>
#include <pspthreadman.h>
#include "../commands/commands.h"

static u32 __attribute__((aligned(16))) dlist1[] = {
	0x00000000, // 0x00 NOP (for GE_CMD_TRANSFERSRC)
	0x00000000, // 0x01 NOP
	0x00000000, // 0x02 NOP (for GE_CMD_TRANSFERSRCPOS)
	0x00000000, // 0x03 NOP (for GE_CMD_TRANSFERDST)
	0x00000000, // 0x04 NOP
	0x00000000, // 0x05 NOP (for GE_CMD_TRANSFERDSTPOS)
	0x00000000, // 0x06 NOP (for GE_CMD_TRANSFERSIZE)
	0xEA000000, // 0x07 TRANSFERSTART
	0x00000000, // 0x08 NOP
	0xCC000000, // 0x09 TEXSYNC
	0xCB000000, // 0x0A TEXFLUSH
	0x0F000000, // 0x0B FINISH
	0x0C000000, // 0x0C END
};

static u32 transferCoords(int cmd, int x, int y) {
	return (cmd << 24) | ((y << 10) & 0x000FFC00) | (x & 0x3FF);
}

static u32 transferAddr1(int cmd, void *p) {
	return (cmd << 24) | ((u32)p & 0x00FFFFFF);
}

static u32 transferAddr2w(int cmd, void *p, int w) {
	return (cmd << 24) | (((u32)p & 0xFF000000) >> 8) | (w & 0x0000FFFF);
}

static void initSrc() {
	sceGeEdramSetAddrTranslation(0);

	u8 *mem1 = (u8 *)0x04000000;
	for (int i = 0; i < 0x4000; ++i) {
		mem1[i] = i & 0xFF;
	}
	sceKernelDcacheWritebackInvalidateRange(mem1, 0x4000);
}

static void testTransferMirrors(const char *title, int fromMirror, int toMirror, int offset = -1, int bpp = 16) {
	u8 *mem1 = (u8 *)(0x04000000 + fromMirror);
	u8 *mem2 = (u8 *)(0x04000000 + toMirror);

	initSrc();

	memset(mem2, 0xFF, 0x4000);
	if (memcmp(mem1, mem2, 1024) == 0) {
		printf("TESTERROR\n");
	}
	sceKernelDcacheWritebackInvalidateRange(mem2, 4096);

	dlist1[0] = transferAddr1(GE_CMD_TRANSFERSRC, mem1);
	dlist1[1] = transferAddr2w(GE_CMD_TRANSFERSRCW, mem1, bpp == 32 ? 256 : 512);
	dlist1[2] = transferCoords(GE_CMD_TRANSFERSRCPOS, 0, 0);
	dlist1[3] = transferAddr1(GE_CMD_TRANSFERDST, mem2);
	dlist1[4] = transferAddr2w(GE_CMD_TRANSFERDSTW, mem2, bpp == 32 ? 256 : 512);
	dlist1[5] = transferCoords(GE_CMD_TRANSFERDSTPOS, 0, 0);
	dlist1[6] = transferCoords(GE_CMD_TRANSFERSIZE, bpp == 32 ? 255 : 511, 15);
	dlist1[7] = (GE_CMD_TRANSFERSTART << 24) | (bpp == 32 ? 1 : 0);

	sceKernelDcacheWritebackInvalidateRange(dlist1, sizeof(dlist1));
	sceGeListEnQueue(dlist1, dlist1 + sizeof(dlist1), -1, NULL);
	sceGeDrawSync(0);

	sceKernelDcacheInvalidateRange(mem1, 0x4000);
	sceKernelDcacheInvalidateRange(mem2, 0x4000);
	if (offset == -1) {
		checkpoint("%s: %d (%p -> %p)", title, memcmp(mem1, mem2, 1024), mem1, mem2);
	} else {
		checkpoint("%s: %d (%p -> %p / %d,%d -> %d,%d)", title, memcmp(mem1, mem2, 1024), mem1, mem2, mem1[offset], mem1[offset + 1], mem2[offset], mem2[offset + 1]);
	}
}

extern "C" int main(int argc, char *argv[]) {
	checkpointNext("Alignment:");
	testTransferMirrors("  Source offset 0", 0x00000000, 0x00100000, 0);
	testTransferMirrors("  Source offset 4", 0x00000004, 0x00100000, 0);
	testTransferMirrors("  Source offset 8", 0x00000008, 0x00100000, 0);
	testTransferMirrors("  Source offset 15", 0x0000000F, 0x00100000, 0);
	testTransferMirrors("  Source offset 16", 0x00000010, 0x00100000, 0);
	testTransferMirrors("  Dest offset 4", 0x00000000, 0x00100004, 0);
	testTransferMirrors("  Dest offset 8", 0x00000000, 0x00100008, 0);
	testTransferMirrors("  Dest offset 15", 0x00000000, 0x0010000F, 0);
	testTransferMirrors("  Dest offset 16", 0x00000000, 0x00100010, 0);

	checkpointNext("Overlap:");
	testTransferMirrors("  Overlap 0x50, 16-bit", 0x00000000, 0x00000050, 0x4F);
	testTransferMirrors("  Overlap 0x40, 16-bit", 0x00000000, 0x00000040, 0x3F);
	testTransferMirrors("  Overlap 0x30, 16-bit", 0x00000000, 0x00000030, 0x2F);
	testTransferMirrors("  Overlap 0x20, 16-bit", 0x00000000, 0x00000020, 0x1F);
	testTransferMirrors("  Overlap 0x10, 16-bit", 0x00000000, 0x00000010, 0x0F);
	testTransferMirrors("  Overlap 0x01, 16-bit", 0x00000000, 0x00000001, 0);
	testTransferMirrors("  Overlap 0x50, 32-bit", 0x00000000, 0x00000050, 0x4F, 32);
	testTransferMirrors("  Overlap 0x40, 32-bit", 0x00000000, 0x00000040, 0x3F, 32);
	testTransferMirrors("  Overlap 0x30, 32-bit", 0x00000000, 0x00000030, 0x2F, 32);
	testTransferMirrors("  Overlap 0x20, 32-bit", 0x00000000, 0x00000020, 0x1F, 32);
	testTransferMirrors("  Overlap 0x10, 32-bit", 0x00000000, 0x00000010, 0x0F, 32);
	testTransferMirrors("  Overlap 0x01, 32-bit", 0x00000000, 0x00000001, 0, 32);

	return 0;
}