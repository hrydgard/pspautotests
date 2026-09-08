#include <common.h>
#include <psputils.h>

// How big is an MD5 context, what's in it, and can you run two at once?
//
// Emulators have historically kept a single global MD5 state and ignored the context pointer the
// caller hands in, which works right up until something interleaves two digests. Nothing tested
// that, or the size of the context, or its contents - so do all three:
//
//   1. Poison a buffer, run BlockInit/Update/Result in the middle of it, and report which bytes
//      the kernel actually touched. That's the context size, measured rather than assumed.
//   2. Interleave two contexts and check both digests still match the one-at-a-time answers.
//      An implementation with one global state gets both wrong.
//   3. MD5 is a public algorithm, so we know what the state has to be after init - the four
//      standard chaining values - and after a whole number of 64-byte blocks. Search the context
//      for them and report where they sit, which pins down the layout without guessing at it.

#define POISON 0xCC
#define MARGIN 32
// Bigger than any plausible context so a write past the end still lands in our buffer.
#define SLOT 512

static const char *text =
	"Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt "
	"ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation.";

// The four MD5 chaining values, which any implementation must start from.
static const unsigned int md5InitState[4] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476 };

static char bufA[MARGIN + SLOT + MARGIN];
static char bufB[MARGIN + SLOT + MARGIN];

static void *slotOf(char *buf) {
	memset(buf, POISON, MARGIN + SLOT + MARGIN);
	return buf + MARGIN;
}

static void printDigest(const char *title, const unsigned char *d) {
	int i;
	printf("%s: ", title);
	for (i = 0; i < 16; i++) {
		printf("%02x", d[i]);
	}
	printf("\n");
}

// Which bytes of the slot did the kernel write? Reported relative to the context pointer, so a
// negative number would mean it scribbled before the buffer it was given.
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

// Locate a known word sequence inside the context, so we learn the layout instead of assuming it.
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

// The first 32 bytes are state plus bookkeeping - dump them so the counter fields are pinned
// down rather than guessed at. Everything here is deterministic for a fixed input.
static void dumpHeader(const char *title, const char *buf) {
	int i;
	printf("%s: ", title);
	for (i = 0; i < 32; i += 4) {
		unsigned int w;
		memcpy(&w, buf + MARGIN + i, 4);
		printf("%08x%s", w, i == 28 ? "\n" : " ");
	}
}

static void digestOneShot(const char *title, const char *data, int len) {
	unsigned char digest[16];
	memset(digest, 0, sizeof(digest));
	sceKernelUtilsMd5Digest((u8 *)data, len, digest);
	printDigest(title, digest);
}

// The reference answer: one context, start to finish, nothing else going on.
static void digestSequential(const char *title, const char *data, int len, unsigned char *out) {
	void *ctx = slotOf(bufA);
	sceKernelUtilsMd5BlockInit(ctx);
	sceKernelUtilsMd5BlockUpdate(ctx, (u8 *)data, len);
	sceKernelUtilsMd5BlockResult(ctx, out);
	printDigest(title, out);
}

int main(int argc, char **argv) {
	int len = strlen(text);
	int half = len / 2;
	unsigned char refA[16], refB[16], gotA[16], gotB[16];
	void *ctxA, *ctxB;

	printf("-- what the SDK header claims:\n");
	printf("sizeof(SceKernelUtilsMd5Context) = %d\n", (int)sizeof(SceKernelUtilsMd5Context));

	printf("-- context size and contents:\n");
	ctxA = slotOf(bufA);
	printf("BlockInit: %08x\n", sceKernelUtilsMd5BlockInit(ctxA));
	reportTouched("after init", bufA);
	findWords("init state (67452301 efcdab89 98badcfe 10325476)", bufA, md5InitState, 4);
	dumpHeader("header after init", bufA);

	// One full block, so the state should have advanced and nothing should be left buffered.
	sceKernelUtilsMd5BlockUpdate(ctxA, (u8 *)text, 64);
	reportTouched("after one 64-byte update", bufA);
	dumpHeader("header after 64 bytes", bufA);
	// A partial block has to be stashed somewhere in the context - find the stashed bytes.
	sceKernelUtilsMd5BlockUpdate(ctxA, (u8 *)text + 64, 5);
	{
		unsigned int want;
		memcpy(&want, text + 64, 4);
		findWords("first 4 bytes of the partial block", bufA, &want, 1);
	}
	reportTouched("after a 5-byte update", bufA);
	dumpHeader("header after 5 more bytes", bufA);

	printf("-- reference digests, one at a time:\n");
	digestOneShot("whole thing, Md5Digest", text, len);
	digestSequential("whole thing, Block*", text, len, refA);
	digestSequential("first half, Block*", text, half, refB);

	printf("-- two contexts, interleaved:\n");
	ctxA = slotOf(bufA);
	ctxB = slotOf(bufB);
	sceKernelUtilsMd5BlockInit(ctxA);
	sceKernelUtilsMd5BlockInit(ctxB);
	// Feed A, then B, then the rest of A, so a shared global state can't come out right.
	sceKernelUtilsMd5BlockUpdate(ctxA, (u8 *)text, half);
	sceKernelUtilsMd5BlockUpdate(ctxB, (u8 *)text, half);
	sceKernelUtilsMd5BlockUpdate(ctxA, (u8 *)text + half, len - half);
	sceKernelUtilsMd5BlockResult(ctxB, gotB);
	sceKernelUtilsMd5BlockResult(ctxA, gotA);
	printDigest("interleaved A (whole thing)", gotA);
	printDigest("interleaved B (first half)", gotB);
	printf("A matches its sequential digest: %d\n", memcmp(gotA, refA, 16) == 0);
	printf("B matches its sequential digest: %d\n", memcmp(gotB, refB, 16) == 0);

	printf("-- a context is self-contained:\n");
	// If the context really holds all the state, copying it mid-digest should carry on fine.
	{
		static char copy[MARGIN + SLOT + MARGIN];
		unsigned char fromOriginal[16], fromCopy[16];
		ctxA = slotOf(bufA);
		sceKernelUtilsMd5BlockInit(ctxA);
		sceKernelUtilsMd5BlockUpdate(ctxA, (u8 *)text, half);
		memcpy(copy, bufA, sizeof(copy));
		sceKernelUtilsMd5BlockUpdate(ctxA, (u8 *)text + half, len - half);
		sceKernelUtilsMd5BlockResult(ctxA, fromOriginal);
		sceKernelUtilsMd5BlockUpdate(copy + MARGIN, (u8 *)text + half, len - half);
		sceKernelUtilsMd5BlockResult(copy + MARGIN, fromCopy);
		printf("copied context finishes identically: %d\n", memcmp(fromOriginal, fromCopy, 16) == 0);
	}

	return 0;
}
