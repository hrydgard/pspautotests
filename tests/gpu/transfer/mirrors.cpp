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

static void testTransferMirrors(const char *title, int fromMirror, int toMirror, int offset = -1, int bpp = 16) {
	u8 *mem1 = (u8 *)(0x04000000 + fromMirror);
	u8 *mem2 = (u8 *)(0x04000000 + toMirror);

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

static void initSrc() {
	sceGeEdramSetAddrTranslation(0);

	u8 *mem1 = (u8 *)0x04000000;
	for (int i = 0; i < 0x4000; ++i) {
		mem1[i] = i & 0xFF;
	}
	sceKernelDcacheWritebackInvalidateRange(mem1, 0x4000);

	// Used for straddle tests.
	u8 *mem3 = (u8 *)0x043FC000;
	for (int i = 0; i < 0x4000; ++i) {
		mem3[i] = 0x80 | (i & 0x7F);
	}
	sceKernelDcacheWritebackInvalidateRange(mem3, 0x4000);
}

extern "C" int main(int argc, char *argv[]) {
	initSrc();

	checkpointNext("Between VRAM mirrors:");
	testTransferMirrors("  Intra primary -> primary", 0x00000000, 0x00100000);
	testTransferMirrors("  Intra primary -> simple", 0x00000000, 0x00300000);
	testTransferMirrors("  Intra primary -> identical", 0x00000000, 0x00500000);
	testTransferMirrors("  Intra primary -> complex", 0x00000000, 0x00700000);

	checkpointNext("Straddling mirrors:");
	testTransferMirrors("  Straddle simple/identical -> primary, 0x200 each side", 0x003FFE00, 0x00100000, 0);
	testTransferMirrors("  Straddle simple/identical -> primary, 0x10 early", 0x003FFFF0, 0x00100000, 0);
	testTransferMirrors("  Straddle identical/complex -> primary, 0x200 each side", 0x005FFE00, 0x00100000, 0);
	testTransferMirrors("  Straddle identical/complex -> primary, 0x10 early", 0x005FFFF0, 0x00100000, 0);

	return 0;
}