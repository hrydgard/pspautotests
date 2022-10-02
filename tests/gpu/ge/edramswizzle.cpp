#include <common.h>
#include <pspge.h>
#include <psputils.h>

extern "C" int sceGeEdramSetAddrTranslation(int value);

static uint8_t *const vram = (uint8_t *)0x04000000;
static uint8_t *const vramMirror1 = (uint8_t *)0x04200000;
static uint8_t *const vramMirror2 = (uint8_t *)0x04400000;
static uint8_t *const vramMirror3 = (uint8_t *)0x04600000;
static const uint32_t vramSize = 0x00200000;

static void logFoundByteSeq(const uint8_t *mirror, int byte, int offset, int n, int value) {
	if (offset == -1) {
		checkpoint("  Mirror %d did not find %02x sequence (%04x)", n, byte, value);
		return;
	}

	int inOrder = 0;
	for (int j = 0; j < 256; ++j) {
		if (mirror[offset + j] != byte + j) {
			break;
		}
		inOrder = j + 1;
	}

	checkpoint(NULL);
	schedf("  Mirror %d found %02x sequence at offset %04x (%04x): ", n, byte, offset, value);
	if (inOrder >= 16) {
		schedf("in order from %02x - %02x (%d bytes)", byte, mirror[offset + inOrder - 1], inOrder);
	} else {
		for (int j = 0; j < 16; ++j) {
			schedf(" %02x", mirror[offset + j]);
		}
	}
	schedf("\n");
}

static void checkMirrorSeq(const uint8_t *mirror, int n, int value) {
	int pos00 = -1;
	int pos20 = -1;
	int pos40 = -1;
	int pos60 = -1;
	int pos80 = -1;
	int posA0 = -1;
	int posC0 = -1;
	int posE0 = -1;

	sceKernelDcacheWritebackInvalidateRange(mirror, vramSize);
	for (uint32_t i = 0; i < vramSize; ++i) {
		switch (mirror[i]) {
		case 0x01:
			if (i > 0 && mirror[i - 1] == 0) {
				pos00 = i - 1;
				if (posE0 != -1 && posC0 != -1 && posA0 != -1 && pos80 != -1 && pos60 != -1 && pos40 != -1 && pos20 != -1) {
					i = vramSize;
				}
			}
			break;

		case 0x20:
			pos20 = i;
			if (posE0 != -1 && posC0 != -1 && posA0 != -1 && pos80 != -1 && pos60 != -1 && pos40 != -1 && pos20 != -1) {
				i = vramSize;
			}
			break;

		case 0x40:
			pos40 = i;
			if (posE0 != -1 && posC0 != -1 && posA0 != -1 && pos80 != -1 && pos60 != -1 && pos40 != -1 && pos20 != -1) {
				i = vramSize;
			}
			break;

		case 0x60:
			pos60 = i;
			if (posE0 != -1 && posC0 != -1 && posA0 != -1 && pos80 != -1 && pos60 != -1 && pos40 != -1 && pos20 != -1) {
				i = vramSize;
			}
			break;

		case 0x80:
			pos80 = i;
			if (posE0 != -1 && posC0 != -1 && posA0 != -1 && pos80 != -1 && pos60 != -1 && pos40 != -1 && pos20 != -1) {
				i = vramSize;
			}
			break;

		case 0xA0:
			posA0 = i;
			if (posE0 != -1 && posC0 != -1 && posA0 != -1 && pos80 != -1 && pos60 != -1 && pos40 != -1 && pos20 != -1) {
				i = vramSize;
			}
			break;

		case 0xC0:
			posC0 = i;
			if (posE0 != -1 && posC0 != -1 && posA0 != -1 && pos80 != -1 && pos60 != -1 && pos40 != -1 && pos20 != -1) {
				i = vramSize;
			}
			break;

		case 0xE0:
			posE0 = i;
			if (posE0 != -1 && posC0 != -1 && posA0 != -1 && pos80 != -1 && pos60 != -1 && pos40 != -1 && pos20 != -1) {
				i = vramSize;
			}
			break;

		default:
			break;
		}
	}

	logFoundByteSeq(mirror, 0x00, pos00, n, value);
	logFoundByteSeq(mirror, 0x20, pos20, n, value);
	logFoundByteSeq(mirror, 0x40, pos40, n, value);
	logFoundByteSeq(mirror, 0x60, pos60, n, value);
	logFoundByteSeq(mirror, 0x80, pos80, n, value);
	logFoundByteSeq(mirror, 0xA0, posA0, n, value);
	logFoundByteSeq(mirror, 0xC0, posC0, n, value);
	logFoundByteSeq(mirror, 0xE0, posE0, n, value);
}

static void checkMirrorExtent(const uint8_t *mirror, int size, int offset, int n, int value) {
	int hits = 0;
	int lastStart = -1;

	sceKernelDcacheWritebackInvalidateRange(mirror, vramSize);
	checkpoint(NULL);
	schedf("  Mirror %d extent for %d bytes from %04x (%04x):", n, size, offset, value);
	for (uint32_t i = 0; i < vramSize; ++i) {
		if (mirror[i] == 0) {
			if (lastStart != -1) {
				schedf(" %04x-%04x", lastStart, i - 1);
				lastStart = -1;
			}
			continue;
		}
		if (mirror[i] != 0xFF) {
			schedf(" unexpected byte %02x\n", mirror[i]);
			return;
		}

		hits++;
		if (lastStart == -1) {
			lastStart = i;
		}
		if (hits >= size) {
			schedf(" %04x-%04x", lastStart, i);
			lastStart = -1;
			break;
		}
	}

	if (lastStart != -1) {
		schedf(" %04x-%04x", lastStart, vramSize - 1);
	}
	schedf("\n");
}

static void testTranslation(int value) {
	char temp[256];
	snprintf(temp, sizeof(temp), "Translation %04x:", value);
	checkpointNext(temp);

	if (sceGeEdramSetAddrTranslation(value) < 0)
		checkpoint("  Failed to set edram translation to %04x", value);
	memset(vram, 0, vramSize);
	sceKernelDcacheWritebackInvalidateAll();
	checkpoint("  Cleared VRAM (%04x)", value);

	checkpoint("  Cleared mirror 1 diff=%d (%04x)", memcmp(vram, vramMirror1, vramSize), value);
	checkpoint("  Cleared mirror 2 diff=%d (%04x)", memcmp(vram, vramMirror2, vramSize), value);
	checkpoint("  Cleared mirror 3 diff=%d (%04x)", memcmp(vram, vramMirror3, vramSize), value);

	for (int i = 0; i < 256; ++i) {
		vram[i] = i;
	}
	sceKernelDcacheWritebackInvalidateRange(vram, 256);

	checkMirrorSeq(vramMirror1, 1, value);
	checkMirrorSeq(vramMirror2, 2, value);
	checkMirrorSeq(vramMirror3, 3, value);

	for (int size = 256; size <= 2048; size += size) {
		memset(vram, 0xFF, size);
		sceKernelDcacheWritebackInvalidateRange(vram, size);

		checkMirrorExtent(vramMirror1, size, 0, 1, value);
		checkMirrorExtent(vramMirror2, size, 0, 2, value);
		checkMirrorExtent(vramMirror3, size, 0, 3, value);
	}

	memset(vram, 0, 2048);
	sceKernelDcacheWritebackInvalidateRange(vram, 2048);

	for (int offset = 0x0200; offset <= 0x0E00; offset += 0x0200) {
		memset(vram + offset, 0xFF, 256);
		sceKernelDcacheWritebackInvalidateRange(vram + offset, 256);

		checkMirrorExtent(vramMirror1, 256, offset, 1, value);
		checkMirrorExtent(vramMirror2, 256, offset, 2, value);
		checkMirrorExtent(vramMirror3, 256, offset, 3, value);

		memset(vram + offset, 0, 256);
		sceKernelDcacheWritebackInvalidateRange(vram + offset, 256);
	}
	for (int offset = 0x1000; offset <= 0x8000; offset += offset) {
		memset(vram + offset, 0xFF, 256);
		sceKernelDcacheWritebackInvalidateRange(vram + offset, 256);

		checkMirrorExtent(vramMirror1, 256, offset, 1, value);
		checkMirrorExtent(vramMirror2, 256, offset, 2, value);
		checkMirrorExtent(vramMirror3, 256, offset, 3, value);

		memset(vram + offset, 0, 256);
		sceKernelDcacheWritebackInvalidateRange(vram + offset, 256);
	}
}

static void generatePattern() {
	uint32_t *vram32 = (uint32_t *)vram;
	for (uint32_t i = 0; i < vramSize / 4; ++i) {
		vram32[i] = rand();
	}
	sceKernelDcacheWritebackInvalidateRange(vram, vramSize);
	sceKernelDcacheWritebackInvalidateRange(vramMirror1, vramSize);
	sceKernelDcacheWritebackInvalidateRange(vramMirror2, vramSize);
	sceKernelDcacheWritebackInvalidateRange(vramMirror3, vramSize);
}

static void validatePattern0000() {
	checkpointNext("Validate pattern (0000):");
	if (sceGeEdramSetAddrTranslation(0) < 0)
		checkpoint("  Failed to set edram translation to %04x", 0);

	for (uint32_t i = 0; i < vramSize; ++i) {
		if (vram[i] != vramMirror1[i ^ 0x0600]) {
			checkpoint("  Mismatch for mirror 1 (0000), %04x != %04x", i, i ^ 0x0600);
			return;
		}
		if (vram[i] != vramMirror2[i]) {
			checkpoint("  Mismatch for mirror 2 (0000), %04x != %04x", i, i);
			return;
		}

		uint32_t mirror3i = i;
		if ((mirror3i & 0x0600) == 0 || (mirror3i & 0x0600) == 0x0600) {
			mirror3i ^= 0x0600;
		}
		if (vram[i] != vramMirror3[mirror3i]) {
			checkpoint("  Mismatch for mirror 3 (0000), %04x != %04x", i, mirror3i);
			return;
		}
	}

	checkpoint("  Matched (0000)");
	return;
}

static void validatePattern0200() {
	checkpointNext("Validate pattern (0200):");
	if (sceGeEdramSetAddrTranslation(0x0200) < 0)
		checkpoint("  Failed to set edram translation to %04x", 0x0200);

	for (uint32_t i = 0; i < vramSize; ++i) {
		if (vram[i] != vramMirror1[i ^ 0x1040]) {
			checkpoint("  Mismatch for mirror 1 (0200), %04x != %04x", i, i ^ 0x1040);
			return;
		}
		if (vram[i] != vramMirror2[i]) {
			checkpoint("  Mismatch for mirror 2 (0200), %04x != %04x", i, i);
			return;
		}

		uint32_t mirror3i = i ^ 0x1000;
		mirror3i = (mirror3i & ~0x01E0) | ((i & 0x180) >> 1) | ((~i & 0x0040) >> 1) | ((i & 0x0020) << 3);
		if (vram[i] != vramMirror3[mirror3i]) {
			checkpoint("  Mismatch for mirror 3 (0200), %04x != %04x", i, mirror3i);
			return;
		}
	}

	checkpoint("  Matched (0200)");
	return;
}

static void validatePattern0400() {
	checkpointNext("Validate pattern (0400):");
	if (sceGeEdramSetAddrTranslation(0x0400) < 0)
		checkpoint("  Failed to set edram translation to %04x", 0x0400);

	for (uint32_t i = 0; i < vramSize; ++i) {
		if (vram[i] != vramMirror1[i ^ 0x2040]) {
			checkpoint("  Mismatch for mirror 1 (0400), %04x != %04x", i, i ^ 0x2040);
			return;
		}
		if (vram[i] != vramMirror2[i]) {
			checkpoint("  Mismatch for mirror 2 (0400), %04x != %04x", i, i);
			return;
		}

		uint32_t mirror3i = i ^ 0x2000;
		mirror3i = (mirror3i & ~0x03E0) | ((i & 0x380) >> 1) | ((~i & 0x0040) >> 1) | ((i & 0x0020) << 4);
		if (vram[i] != vramMirror3[mirror3i]) {
			checkpoint("  Mismatch for mirror 3 (0400), %04x != %04x", i, mirror3i);
			return;
		}
	}

	checkpoint("  Matched (0400)");
	return;
}

static void validatePattern0800() {
	checkpointNext("Validate pattern (0800):");
	if (sceGeEdramSetAddrTranslation(0x0800) < 0)
		checkpoint("  Failed to set edram translation to %04x", 0x0800);

	for (uint32_t i = 0; i < vramSize; ++i) {
		if (vram[i] != vramMirror1[i ^ 0x4040]) {
			checkpoint("  Mismatch for mirror 1 (0800), %04x != %04x", i, i ^ 0x4040);
			return;
		}
		if (vram[i] != vramMirror2[i]) {
			checkpoint("  Mismatch for mirror 2 (0800), %04x != %04x", i, i);
			return;
		}

		uint32_t mirror3i = i ^ 0x4000;
		mirror3i = (mirror3i & ~0x07E0) | ((i & 0x780) >> 1) | ((~i & 0x0040) >> 1) | ((i & 0x0020) << 5);
		if (vram[i] != vramMirror3[mirror3i]) {
			checkpoint("  Mismatch for mirror 3 (0800), %04x != %04x", i, mirror3i);
			return;
		}
	}

	checkpoint("  Matched (0800)");
	return;
}

static void validatePattern1000() {
	checkpointNext("Validate pattern (1000):");
	if (sceGeEdramSetAddrTranslation(0x1000) < 0)
		checkpoint("  Failed to set edram translation to %04x", 0x1000);

	for (uint32_t i = 0; i < vramSize; ++i) {
		if (vram[i] != vramMirror1[i ^ 0x8040]) {
			checkpoint("  Mismatch for mirror 1 (1000), %04x != %04x", i, i ^ 0x8040);
			return;
		}
		if (vram[i] != vramMirror2[i]) {
			checkpoint("  Mismatch for mirror 2 (1000), %04x != %04x", i, i);
			return;
		}

		uint32_t mirror3i = i ^ 0x8000;
		mirror3i = (mirror3i & ~0x0FE0) | ((i & 0xF80) >> 1) | ((~i & 0x0040) >> 1) | ((i & 0x0020) << 6);
		if (vram[i] != vramMirror3[mirror3i]) {
			checkpoint("  Mismatch for mirror 3 (1000), %04x != %04x", i, mirror3i);
			return;
		}
	}

	checkpoint("  Matched (1000)");
	return;
}

extern "C" int main(int argc, char *argv[]) {
	testTranslation(0);
	for (int i = 0x200; i <= 0x1000; i += i) {
		testTranslation(i);
	}

	generatePattern();
	validatePattern0000();
	validatePattern0200();
	validatePattern0400();
	validatePattern0800();
	validatePattern1000();

	return 0;
}