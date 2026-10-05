#include <cstring>
#include <cstdlib>
#include <malloc.h>
#include "jobs.h"

// The job format is that of ppsspp-re's tools/geprobe/probe.cpp (see write_jobs in geprobe.py): per job,
// u32 rows (upper 16 bits: EDRAM address translation + 1, or 0), u32 words, u32 readback (bit 31: 16-bit
// pixels, bit 30: read with the CPU, low bits: VRAM offset, maybe in a mirror), u32 relocs, the list words,
// the relocation word indices, then a pre list in the same form (u32 words, u32 relocs, none if words is
// 0), then u32 writes and per write u32 VRAM offset, u32 bytes, the bytes padded to 4.

extern "C" int sceGeEdramSetAddrTranslation(int value);
extern "C" int HAS_DISPLAY;

static void relocate(u32 *words, u32 count, const u32 *relocs, u32 relocCount) {
	const u32 base = (u32)words;
	for (u32 r = 0; r < relocCount; ++r) {
		const u32 i = relocs[r];
		if (i + 1 >= count) {
			continue;
		}
		const u32 w = words[i];
		const u32 addr = base + words[i + 1];
		const u32 kind = (w >> 16) & 0xFF;
		if (kind == 0x01) {
			words[i] = (0x10 << 24) | ((addr >> 8) & 0x0F0000);
			words[i + 1] = (0x01 << 24) | (addr & 0xFFFFFF);
		} else if (kind == 0x03) {
			words[i] = (0x10 << 24) | ((addr >> 8) & 0x0F0000);
			words[i + 1] = (0x02 << 24) | (addr & 0xFFFFFF);
		} else if (kind == 0x02) {
			words[i] = (0xA0 << 24) | (addr & 0xFFFFFF);
			words[i + 1] = (0xA8 << 24) | ((addr >> 8) & 0x0F0000) | (w & 0xFFFF);
		} else if (kind == 0x04) {
			words[i] = (0xB2 << 24) | (addr & 0xFFFFFF);
			words[i + 1] = (0xB3 << 24) | ((addr >> 8) & 0xFF0000) | (w & 0xFFFF);
		} else if (kind == 0x05) {
			words[i] = (0xB1 << 24) | ((addr >> 8) & 0xFF0000);
			words[i + 1] = (0xB0 << 24) | (addr & 0xFFFFFF);
		} else if ((kind & 0xF8) == 0x20) {
			// Texture level n (0x20 + n): address and buffer width.
			const u32 level = kind & 7;
			words[i] = ((0xA0 + level) << 24) | (addr & 0xFFFFFF);
			words[i + 1] = ((0xA8 + level) << 24) | ((addr >> 8) & 0x0F0000) | (w & 0xFFFF);
		}
	}
}

static u32 crcTable[256];

static void initCrc() {
	for (u32 i = 0; i < 256; ++i) {
		u32 c = i;
		for (int k = 0; k < 8; ++k) {
			c = (c & 1) ? 0xEDB88320 ^ (c >> 1) : c >> 1;
		}
		crcTable[i] = c;
	}
}

static u32 crc32(u32 crc, const u8 *p, u32 len) {
	crc = ~crc;
	for (u32 i = 0; i < len; ++i) {
		crc = crcTable[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
	}
	return ~crc;
}

static u32 readU32(const u8 *&p) {
	u32 v;
	memcpy(&v, p, 4);
	p += 4;
	return v;
}

// A list (and its relocations) from the job data, copied to an aligned buffer and relocated there.
static u32 *readList(const u8 *&p, u32 words, u32 relocCount) {
	u32 *list = (u32 *)memalign(64, words * 4 + 64);
	memcpy(list, p, words * 4);
	p += words * 4;
	u32 *relocs = (u32 *)malloc(relocCount * 4 + 4);
	memcpy(relocs, p, relocCount * 4);
	p += relocCount * 4;
	relocate(list, words, relocs, relocCount);
	free(relocs);
	return list;
}

int runJobs(const u8 *data, const ExactJob *jobs, int count) {
	initDisplay();
	// Printed text would also be drawn into the framebuffer, which is the render target here.
	HAS_DISPLAY = 0;
	initCrc();

	const u8 *p = data;
	const u32 jobCount = readU32(p);
	if ((int)jobCount != count) {
		printf("Job count mismatch: %d in the data, %d named\n", (int)jobCount, count);
		return 1;
	}
	int origTranslation = -1;
	u32 *readBuf = (u32 *)memalign(64, 512 * 4 * 272);
	for (u32 j = 0; j < jobCount; ++j) {
		u32 rows = readU32(p);
		const u32 words = readU32(p);
		const u32 readback = readU32(p);
		const u32 relocCount = readU32(p);
		if (rows >> 16) {
			const int prev = sceGeEdramSetAddrTranslation((rows >> 16) - 1);
			if (origTranslation < 0) {
				origTranslation = prev;
			}
		}
		rows &= 0xFFFF;
		const bool read16 = (readback & 0x80000000) != 0;
		const bool readCPU = (readback & 0x40000000) != 0;
		const u32 readOffset = readback & 0x3FFFFFFF;
		const u32 bpp = read16 ? 2 : 4;
		u32 *list = readList(p, words, relocCount);

		const u32 preWords = readU32(p);
		const u32 preRelocs = readU32(p);
		u32 *pre = preWords ? readList(p, preWords, preRelocs) : NULL;
		const u32 writeCount = readU32(p);
		sceKernelDcacheWritebackInvalidateAll();

		sceGuStart(GU_DIRECT, ::list);
		sceGuClearColor(0);
		sceGuClearDepth(0);
		sceGuClearStencil(0);
		sceGuClear(GU_COLOR_BUFFER_BIT | GU_STENCIL_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
		if (pre) {
			sceGuCallList(pre);
		}
		if (writeCount) {
			sceGuFinish();
			sceGuSync(0, 0);
			for (u32 w = 0; w < writeCount; ++w) {
				const u32 offset = readU32(p);
				const u32 bytes = readU32(p);
				memcpy((u8 *)(0x44000000 + offset), p, bytes);
				p += (bytes + 3) & ~3;
			}
			sceGuStart(GU_DIRECT, ::list);
		}
		sceGuCallList(list);
		sceGuFinish();
		sceGuSync(0, 0);

		if (readCPU) {
			// Through the uncached mirror, e.g. to read depth deswizzled at 0x200000 or 0x600000. 16 bits at a
			// time, as games read depth: PPSSPP only deswizzles those accesses.
			const volatile u16 *src = (const volatile u16 *)(0x44000000 + readOffset);
			u16 *dst = (u16 *)readBuf;
			for (u32 i = 0; i < rows * 512 * bpp / 2; ++i)
				dst[i] = src[i];
		} else {
			// A block transfer to RAM, so emulators with the framebuffer on a GPU download it.
			sceKernelDcacheWritebackInvalidateRange(readBuf, 512 * 4 * 272);
			sceGuStart(GU_DIRECT, ::list);
			sceGuCopyImage(read16 ? GU_PSM_5650 : GU_PSM_8888, 0, 0, 512, rows, 512, (u8 *)sceGeEdramGetAddr() + readOffset, 0, 0, 512, readBuf);
			sceGuTexSync();
			sceGuFinish();
			sceGuSync(0, 0);
			sceKernelDcacheWritebackInvalidateRange(readBuf, 512 * 4 * 272);
		}
		free(list);
		free(pre);

		// The first 480 pixels of each row: the clear doesn't reach past them.
		u32 crc = 0;
		for (u32 y = 0; y < rows; ++y) {
			crc = crc32(crc, (const u8 *)readBuf + y * 512 * bpp, 480 * bpp);
		}
		// Not checkpoint(): whether the thread was rescheduled in between depends on GE timing.
		printf("%s: %08x\n", jobs[j].name, (unsigned int)crc);
	}
	if (origTranslation >= 0) {
		sceGeEdramSetAddrTranslation(origTranslation);
	}
	free(readBuf);
	return 0;
}
