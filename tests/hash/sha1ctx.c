#include <common.h>
#include <psputils.h>

// The SHA-1 half of what hash/md5ctx asks about MD5: how big is the context, what's in it, and
// does interleaving two of them work. Emulators that keep one global SHA-1 state and ignore the
// context pointer pass every existing test and fail this one.

#define POISON 0xCC
#define MARGIN 32
#define SLOT 512

static const char *text =
	"Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt "
	"ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation.";

// The five SHA-1 chaining values every implementation starts from.
static const unsigned int sha1InitState[5] = {
	0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0
};

static char bufA[MARGIN + SLOT + MARGIN];
static char bufB[MARGIN + SLOT + MARGIN];

static void *slotOf(char *buf) {
	memset(buf, POISON, MARGIN + SLOT + MARGIN);
	return buf + MARGIN;
}

static void printDigest(const char *title, const unsigned char *d) {
	int i;
	printf("%s: ", title);
	for (i = 0; i < 20; i++) {
		printf("%02x", d[i]);
	}
	printf("\n");
}

static void reportTouched(const char *title, const char *buf) {
	int first = -1, last = -1;
	int i;
	for (i = 0; i < MARGIN + SLOT + MARGIN; i++) {
		if ((unsigned char)buf[i] != POISON) {
			if (first < 0) {
				first = i;
			}
			last = i;
		}
	}
	if (first < 0) {
		printf("%s: context untouched\n", title);
	} else {
		printf("%s: touched [%d..%d], so at least %d bytes\n", title,
			first - MARGIN, last - MARGIN, last - MARGIN + 1);
	}
}

static void findWords(const char *title, const char *buf, const unsigned int *want, int count) {
	int limit = MARGIN + SLOT + MARGIN - (int)sizeof(unsigned int) * count;
	int i;
	for (i = 0; i <= limit; i += 4) {
		if (memcmp(buf + i, want, sizeof(unsigned int) * count) == 0) {
			printf("%s: found at offset %d\n", title, i - MARGIN);
			return;
		}
	}
	printf("%s: not found\n", title);
}

// State plus bookkeeping, so the counter fields are pinned down rather than guessed at.
static void dumpHeader(const char *title, const char *buf) {
	int i;
	printf("%s: ", title);
	for (i = 0; i < 32; i += 4) {
		unsigned int w;
		memcpy(&w, buf + MARGIN + i, 4);
		printf("%08x%s", w, i == 28 ? "\n" : " ");
	}
}

static void digestSequential(const char *title, const char *data, int len, unsigned char *out) {
	void *ctx = slotOf(bufA);
	sceKernelUtilsSha1BlockInit(ctx);
	sceKernelUtilsSha1BlockUpdate(ctx, (u8 *)data, len);
	sceKernelUtilsSha1BlockResult(ctx, out);
	printDigest(title, out);
}

int main(int argc, char **argv) {
	int len = strlen(text);
	int half = len / 2;
	unsigned char refA[20], refB[20], gotA[20], gotB[20], digest[20];
	void *ctxA, *ctxB;

	printf("-- what the SDK header claims:\n");
	printf("sizeof(SceKernelUtilsSha1Context) = %d\n", (int)sizeof(SceKernelUtilsSha1Context));

	printf("-- context size and contents:\n");
	ctxA = slotOf(bufA);
	printf("BlockInit: %08x\n", sceKernelUtilsSha1BlockInit(ctxA));
	reportTouched("after init", bufA);
	findWords("init state (67452301 ... c3d2e1f0)", bufA, sha1InitState, 5);
	dumpHeader("header after init", bufA);

	sceKernelUtilsSha1BlockUpdate(ctxA, (u8 *)text, 64);
	reportTouched("after one 64-byte update", bufA);
	dumpHeader("header after 64 bytes", bufA);

	sceKernelUtilsSha1BlockUpdate(ctxA, (u8 *)text + 64, 5);
	{
		unsigned int want;
		memcpy(&want, text + 64, 4);
		findWords("first 4 bytes of the partial block", bufA, &want, 1);
	}
	reportTouched("after a 5-byte update", bufA);
	dumpHeader("header after 5 more bytes", bufA);

	printf("-- reference digests, one at a time:\n");
	memset(digest, 0, sizeof(digest));
	sceKernelUtilsSha1Digest((u8 *)text, len, digest);
	printDigest("whole thing, Sha1Digest", digest);
	digestSequential("whole thing, Block*", text, len, refA);
	digestSequential("first half, Block*", text, half, refB);

	printf("-- two contexts, interleaved:\n");
	ctxA = slotOf(bufA);
	ctxB = slotOf(bufB);
	sceKernelUtilsSha1BlockInit(ctxA);
	sceKernelUtilsSha1BlockInit(ctxB);
	sceKernelUtilsSha1BlockUpdate(ctxA, (u8 *)text, half);
	sceKernelUtilsSha1BlockUpdate(ctxB, (u8 *)text, half);
	sceKernelUtilsSha1BlockUpdate(ctxA, (u8 *)text + half, len - half);
	sceKernelUtilsSha1BlockResult(ctxB, gotB);
	sceKernelUtilsSha1BlockResult(ctxA, gotA);
	printDigest("interleaved A (whole thing)", gotA);
	printDigest("interleaved B (first half)", gotB);
	printf("A matches its sequential digest: %d\n", memcmp(gotA, refA, 20) == 0);
	printf("B matches its sequential digest: %d\n", memcmp(gotB, refB, 20) == 0);

	printf("-- a context is self-contained:\n");
	{
		static char copy[MARGIN + SLOT + MARGIN];
		unsigned char fromOriginal[20], fromCopy[20];
		ctxA = slotOf(bufA);
		sceKernelUtilsSha1BlockInit(ctxA);
		sceKernelUtilsSha1BlockUpdate(ctxA, (u8 *)text, half);
		memcpy(copy, bufA, sizeof(copy));
		sceKernelUtilsSha1BlockUpdate(ctxA, (u8 *)text + half, len - half);
		sceKernelUtilsSha1BlockResult(ctxA, fromOriginal);
		sceKernelUtilsSha1BlockUpdate(copy + MARGIN, (u8 *)text + half, len - half);
		sceKernelUtilsSha1BlockResult(copy + MARGIN, fromCopy);
		printf("copied context finishes identically: %d\n", memcmp(fromOriginal, fromCopy, 20) == 0);
	}

	return 0;
}
