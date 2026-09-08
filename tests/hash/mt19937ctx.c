#include <common.h>
#include <psputils.h>

// How big is a Mersenne Twister context, what's in it, and can you run two at once?
//
// Same three questions as md5ctx, and the same reason to ask them: an implementation that keeps
// one global generator and ignores the context pointer looks fine until two are in flight.
//
// MT19937 is fully specified, so we can do better than "does it look random" - the state after
// seeding is exactly mt[0] = seed, mt[i] = 1812433253 * (mt[i-1] ^ (mt[i-1] >> 30)) + i. We
// compute that here and check it word for word against what the kernel wrote.

#define POISON 0xCC
#define MARGIN 32
// 624 words of state plus an index, plus room to notice an overrun.
#define SLOT 4096
#define MT_N 624

static char bufA[MARGIN + SLOT + MARGIN];
static char bufB[MARGIN + SLOT + MARGIN];

static unsigned int reference[MT_N];
static unsigned int twisted[MT_N];

static void *slotOf(char *buf) {
	memset(buf, POISON, MARGIN + SLOT + MARGIN);
	return buf + MARGIN;
}

static void buildReference(unsigned int seed) {
	int i;
	reference[0] = seed;
	for (i = 1; i < MT_N; i++) {
		reference[i] = 1812433253u * (reference[i - 1] ^ (reference[i - 1] >> 30)) + i;
	}
	// The generator twists the whole array before handing out the first number. Whether the
	// kernel does that during Init or on the first draw is exactly what we're looking for, so
	// keep both the freshly seeded array and the twisted one to compare against.
	for (i = 0; i < MT_N; i++) {
		twisted[i] = reference[i];
	}
	for (i = 0; i < MT_N; i++) {
		unsigned int y = (twisted[i] & 0x80000000u) | (twisted[(i + 1) % MT_N] & 0x7FFFFFFFu);
		twisted[i] = twisted[(i + 397) % MT_N] ^ (y >> 1);
		if (y & 1) {
			twisted[i] ^= 0x9908B0DFu;
		}
	}
}

// MT19937's output tempering, so we can predict the numbers rather than just record them.
static unsigned int temper(unsigned int y) {
	y ^= y >> 11;
	y ^= (y << 7) & 0x9D2C5680u;
	y ^= (y << 15) & 0xEFC60000u;
	y ^= y >> 18;
	return y;
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

// Find the seeded state array and say where it starts, which also tells us what sits in front
// of it - the SDK header calls that field "count".
static int findState(const char *title, const char *buf) {
	int limit = MARGIN + SLOT + MARGIN - (int)sizeof(reference);
	int i;
	for (i = 0; i <= limit; i += 4) {
		if (memcmp(buf + i, reference, sizeof(reference)) == 0) {
			printf("%s: freshly seeded array, all %d words, at offset %d\n", title, MT_N, i - MARGIN);
			return i;
		}
		if (memcmp(buf + i, twisted, sizeof(twisted)) == 0) {
			printf("%s: already-twisted array, all %d words, at offset %d\n", title, MT_N, i - MARGIN);
			return i;
		}
	}
	// Not an exact match - say how far the run does go, so a partial match is still informative.
	for (i = 0; i <= limit; i += 4) {
		if (memcmp(buf + i, reference, sizeof(unsigned int) * 4) == 0) {
			int words = 0;
			while (words < MT_N && memcmp(buf + i + words * 4, &reference[words], 4) == 0) {
				words++;
			}
			printf("%s: matches only the first %d of %d words, at offset %d\n",
				title, words, MT_N, i - MARGIN);
			return i;
		}
	}
	printf("%s: seeded state not found\n", title);
	return -1;
}

static void drawSome(const char *title, void *ctx, int count) {
	int i;
	printf("%s:", title);
	for (i = 0; i < count; i++) {
		printf(" %08x", sceKernelUtilsMt19937UInt(ctx));
	}
	printf("\n");
}

int main(int argc, char **argv) {
	const unsigned int seedA = 0x12345678;
	const unsigned int seedB = 0xDEADBEEF;
	unsigned int seqA[8], seqB[8], gotA[8], gotB[8];
	void *ctxA, *ctxB;
	int i, stateOffset;

	printf("-- what the SDK header claims:\n");
	printf("sizeof(SceKernelUtilsMt19937Context) = %d\n", (int)sizeof(SceKernelUtilsMt19937Context));

	printf("-- context size and contents:\n");
	ctxA = slotOf(bufA);
	printf("Mt19937Init: %08x\n", sceKernelUtilsMt19937Init(ctxA, seedA));
	reportTouched("after init", bufA);
	buildReference(seedA);
	stateOffset = findState("seeded state", bufA);
	if (stateOffset > MARGIN) {
		// Whatever sits in front of the state is the counter the SDK header calls "count".
		unsigned int before;
		memcpy(&before, bufA + stateOffset - 4, 4);
		printf("word before the state: %08x\n", before);
	}

	// Drawing a number has to advance something - see what moves.
	sceKernelUtilsMt19937UInt(ctxA);
	if (stateOffset > MARGIN) {
		unsigned int before;
		memcpy(&before, bufA + stateOffset - 4, 4);
		printf("word before the state after one draw: %08x\n", before);
	}
	printf("state unchanged by one draw: %d\n",
		stateOffset >= 0 && memcmp(bufA + stateOffset, twisted, sizeof(twisted)) == 0);

	printf("-- reference sequences, one generator at a time:\n");
	ctxA = slotOf(bufA);
	sceKernelUtilsMt19937Init(ctxA, seedA);
	for (i = 0; i < 8; i++) {
		seqA[i] = sceKernelUtilsMt19937UInt(ctxA);
	}
	ctxB = slotOf(bufB);
	sceKernelUtilsMt19937Init(ctxB, seedB);
	for (i = 0; i < 8; i++) {
		seqB[i] = sceKernelUtilsMt19937UInt(ctxB);
	}
	printf("seed %08x:", seedA);
	for (i = 0; i < 8; i++) {
		printf(" %08x", seqA[i]);
	}
	printf("\n");
	printf("seed %08x:", seedB);
	for (i = 0; i < 8; i++) {
		printf(" %08x", seqB[i]);
	}
	printf("\n");

	// MT19937 is fully specified, so say outright whether this is really MT19937.
	buildReference(seedA);
	{
		int matches = 1;
		for (i = 0; i < 8; i++) {
			if (seqA[i] != temper(twisted[i])) {
				matches = 0;
			}
		}
		printf("seed %08x matches reference MT19937: %d\n", seedA, matches);
	}
	buildReference(seedB);
	{
		int matches = 1;
		for (i = 0; i < 8; i++) {
			if (seqB[i] != temper(twisted[i])) {
				matches = 0;
			}
		}
		printf("seed %08x matches reference MT19937: %d\n", seedB, matches);
	}
	buildReference(seedA);

	printf("-- two generators, interleaved:\n");
	ctxA = slotOf(bufA);
	ctxB = slotOf(bufB);
	sceKernelUtilsMt19937Init(ctxA, seedA);
	sceKernelUtilsMt19937Init(ctxB, seedB);
	for (i = 0; i < 8; i++) {
		gotA[i] = sceKernelUtilsMt19937UInt(ctxA);
		gotB[i] = sceKernelUtilsMt19937UInt(ctxB);
	}
	printf("A matches its solo sequence: %d\n", memcmp(gotA, seqA, sizeof(seqA)) == 0);
	printf("B matches its solo sequence: %d\n", memcmp(gotB, seqB, sizeof(seqB)) == 0);
	if (memcmp(gotA, seqA, sizeof(seqA)) != 0) {
		printf("A interleaved:");
		for (i = 0; i < 8; i++) {
			printf(" %08x", gotA[i]);
		}
		printf("\n");
	}

	printf("-- re-seeding the same context:\n");
	sceKernelUtilsMt19937Init(ctxA, seedA);
	drawSome("after re-init with the first seed", ctxA, 8);

	printf("-- a context is self-contained:\n");
	{
		static char copy[MARGIN + SLOT + MARGIN];
		unsigned int fromOriginal[4], fromCopy[4];
		ctxA = slotOf(bufA);
		sceKernelUtilsMt19937Init(ctxA, seedA);
		for (i = 0; i < 3; i++) {
			sceKernelUtilsMt19937UInt(ctxA);
		}
		memcpy(copy, bufA, sizeof(copy));
		for (i = 0; i < 4; i++) {
			fromOriginal[i] = sceKernelUtilsMt19937UInt(ctxA);
		}
		for (i = 0; i < 4; i++) {
			fromCopy[i] = sceKernelUtilsMt19937UInt(copy + MARGIN);
		}
		printf("copied context continues identically: %d\n",
			memcmp(fromOriginal, fromCopy, sizeof(fromOriginal)) == 0);
	}

	return 0;
}
