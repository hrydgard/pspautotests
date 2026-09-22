#include <common.h>
#include <string.h>

// The cache instruction on the data cache, seen through the uncached mirror at 0x40000000: the
// cache is write-back, so a store through a cached pointer isn't in memory until the line is
// written back (0x1a hit writeback, 0x1b hit writeback invalidate), a hit invalidate (0x19)
// drops a dirty line so the old memory contents come back, and a store through the mirror is
// invisible to a cached load until the line is invalidated. An emulator with no data cache sees
// memory immediately in every case, which is why this is documentation more than a test.

static unsigned int __attribute__((aligned(64))) buf[32];

#define CACHE(op, ptr) asm volatile ("cache " #op ", 0(%0)" : : "r" (ptr) : "memory")

static volatile unsigned int *uncached(volatile unsigned int *p) {
	return (volatile unsigned int *)((unsigned int)p | 0x40000000);
}

static void flush(volatile unsigned int *p) {
	CACHE(0x1b, p);
}

int main(int argc, char *argv[]) {
	volatile unsigned int *c = &buf[0];
	volatile unsigned int *u = uncached(c);

	// Start with the line written back and out of the cache, memory = 1.
	*c = 1;
	flush(c);
	printf("start: cached=%u uncached=%u\n", *c, *u);

	// A cached store: memory keeps the old value until the line is written back.
	*c = 2;
	printf("after cached store: cached=%u uncached=%u\n", *c, *u);
	CACHE(0x1a, c);
	printf("after hit writeback (0x1a): cached=%u uncached=%u\n", *c, *u);

	// Another store, then hit invalidate without writeback: the store is lost.
	*c = 3;
	CACHE(0x19, c);
	printf("after store + hit invalidate (0x19): cached=%u uncached=%u\n", *c, *u);

	// Store then writeback invalidate.
	*c = 4;
	CACHE(0x1b, c);
	printf("after store + hit writeback invalidate (0x1b): cached=%u uncached=%u\n", *c, *u);

	// A store through the mirror while the line is cached: the cached load doesn't see it.
	(void)*c;
	*u = 5;
	printf("after uncached store, line cached: cached=%u uncached=%u\n", *c, *u);
	CACHE(0x19, c);
	printf("after hit invalidate: cached=%u uncached=%u\n", *c, *u);

	// Two words on the same line: writing back one address writes back the whole line.
	*c = 6;
	c[1] = 7;
	CACHE(0x1a, c + 1);
	printf("line granularity: uncached[0]=%u uncached[1]=%u\n", u[0], u[1]);

	// Words on the next line aren't touched by an op on this one.
	c[16] = 8;
	CACHE(0x1a, c);
	printf("next line untouched: uncached[16]=%u\n", u[16]);
	flush(c + 16);
	printf("next line written back: uncached[16]=%u\n", u[16]);

	// Ops on a line that isn't in the cache do nothing visible.
	flush(c);
	CACHE(0x1a, c);
	CACHE(0x19, c);
	CACHE(0x1b, c);
	printf("ops on an uncached line: cached=%u uncached=%u\n", *c, *u);
	return 0;
}
