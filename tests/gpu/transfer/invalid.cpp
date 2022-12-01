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

	u8 *mem2 = (u8 *)0x04100000;
	for (int i = 0; i < 0x4000; ++i) {
		mem2[i] = i & 0xFF;
	}
	sceKernelDcacheWritebackInvalidateRange(mem2, 0x4000);

	u8 *mem3 = (u8 *)0x043FC000;
	for (int i = 0; i < 0x4000; ++i) {
		mem3[i] = 0x80 | (i & 0x7F);
	}
	sceKernelDcacheWritebackInvalidateRange(mem3, 0x4000);
}

static void testTransferMirrors(const char *title, int fromMirror, int toMirror, int offset = -1, int bpp = 16, int checksize = 1024) {
	u8 *mem1 = (u8 *)(0x04000000 + fromMirror);
	u8 *mem2 = (u8 *)(0x04000000 + toMirror);
	u8 *wrap = (u8 *)0x04000000;
	bool fromInvalid = 0x04000000 + fromMirror + checksize > 0x04800000;
	bool toInvalid = 0x04000000 + toMirror + checksize > 0x04800000;

	// Do this every time in case we overwrite during wrap.
	initSrc();

	if (checksize != 0) {
		if (fromInvalid && toInvalid) {
			printf("TESTERROR\n");
		} else if (toInvalid) {
			int validToSize = 0x00800000 - toMirror;
			int wrappedToSize = checksize - validToSize;

			memset(mem2, 0xFF, validToSize);
			memset(wrap, 0xFF, wrappedToSize);
			if ((validToSize > 0 && memcmp(mem1, mem2, validToSize) == 0) || memcmp(mem1 + validToSize, wrap, wrappedToSize) == 0) {
				printf("TESTERROR\n");
			}
			sceKernelDcacheWritebackInvalidateRange(mem2, 4096);
			sceKernelDcacheWritebackInvalidateRange(wrap, 4096);
		} else {
			memset(mem2, 0xFF, checksize);
			if (!fromInvalid && memcmp(mem1, mem2, checksize) == 0) {
				printf("TESTERROR\n");
			}
			sceKernelDcacheWritebackInvalidateRange(mem2, 4096);
		}
	}

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
	sceKernelDcacheInvalidateRange(wrap, 0x4000);

	if (checksize == 0) {
		checkpoint("%s: (%p -> %p)", title, mem1, mem2);
	} else if (fromInvalid || toInvalid) {
		int validFromSize = 0x00800000 - fromMirror;
		int wrappedFromSize = checksize - validFromSize;
		int validToSize = 0x00800000 - toMirror;
		int wrappedToSize = checksize - validToSize;

		int cmp1, cmp2;
		if (fromInvalid && toInvalid) {
			printf("TESTERROR\n");
		} else if (fromInvalid) {
			cmp1 = memcmp(mem1, mem2, validFromSize);
			cmp2 = memcmp(wrap, mem2 + validFromSize, wrappedFromSize);
		} else {
			cmp1 = memcmp(mem1, mem2, validToSize);
			cmp2 = memcmp(mem1 + validToSize, wrap, wrappedToSize);
		}

		if (offset != -1) {
			int src1, src2;
			if (offset + 1 < validFromSize) {
				src1 = mem1[offset];
				src2 = mem1[offset + 1];
			} else if (offset + 1 == validFromSize) {
				src1 = mem1[offset];
				src2 = wrap[0];
			} else {
				src1 = wrap[offset - validFromSize];
				src2 = wrap[offset - validFromSize + 1];
			}

			int dst1, dst2;
			if (offset + 1 < validToSize) {
				dst1 = mem2[offset];
				dst2 = mem2[offset + 1];
			} else if (offset + 1 == validToSize) {
				dst1 = mem2[offset];
				dst2 = wrap[0];
			} else {
				dst1 = wrap[offset - validToSize];
				dst2 = wrap[offset - validToSize + 1];
			}

			checkpoint("%s: %d/%d (%p -> %p / %d,%d -> %d,%d)", title, cmp1, cmp2, mem1, mem2, src1, src2, dst1, dst2);
		} else {
			checkpoint("%s: %d/%d (%p -> %p)", title, cmp1, cmp2, mem1, mem2);
		}
	} else if (offset == -1) {
		checkpoint("%s: %d (%p -> %p)", title, memcmp(mem1, mem2, checksize), mem1, mem2);
	} else {
		checkpoint("%s: %d (%p -> %p / %d,%d -> %d,%d)", title, memcmp(mem1, mem2, checksize), mem1, mem2, mem1[offset], mem1[offset + 1], mem2[offset], mem2[offset + 1]);
	}
}

extern "C" int main(int argc, char *argv[]) {
	checkpointNext("Read beyond VRAM end:");
	// This goes over because we use height=16.
	testTransferMirrors("  1/16 lines valid -> valid, 16-bit", 0x007FFC00, 0x00100000, 0, 16);
	testTransferMirrors("  1/16 lines valid -> valid, 32-bit", 0x007FFC00, 0x00100000, 0, 32);
	testTransferMirrors("  16 bytes valid -> valid, 16-bit", 0x007FFFF0, 0x00100000, 16, 16);
	testTransferMirrors("  16 bytes valid -> valid, 32-bit", 0x007FFFF0, 0x00100000, 16, 32);
	testTransferMirrors("  0 bytes valid -> valid, 16-bit", 0x00800000, 0x00100000, 0, 16);
	testTransferMirrors("  0 bytes valid -> valid, 32-bit", 0x00800000, 0x00100000, 0, 32);
	// Crashes/hangs (probably drawsync.)
	//testTransferMirrors("  Outside mask -> valid, 32-bit", 0x01000000, 0x00100000, 0, 32);

	checkpointNext("Write beyond VRAM end:");
	testTransferMirrors("  Valid -> 1/16 lines valid, 16-bit", 0x00100000, 0x007FFC00, 0, 16);
	testTransferMirrors("  Valid -> 1/16 lines valid, 32-bit", 0x00100000, 0x007FFC00, 0, 32);
	testTransferMirrors("  Valid -> 16 bytes valid, 16-bit", 0x00100000, 0x007FFFF0, 16, 16);
	testTransferMirrors("  Valid -> 16 bytes valid, 32-bit", 0x00100000, 0x007FFFF0, 16, 32);
	testTransferMirrors("  Valid -> 0 bytes valid, 16-bit", 0x00100000, 0x00800000, 0, 16);
	testTransferMirrors("  Valid -> 0 bytes valid, 32-bit", 0x00100000, 0x00800000, 0, 32);

	return 0;
}