#pragma once

#include <common.h>
#include <stdio.h>
#include <string.h>

// Prints a 480x272 buffer (stride 512) exactly, but compactly: each row as runs of "value*count"
// (just "value" for a run of one), in hex, with short repeating groups as "(a b)*count", and
// consecutive identical rows once, as "first-last:".
static char dumpLine[480 * 20 + 32];

static char *dumpHex(char *p, u32 v) {
	char digits[8];
	int n = 0;
	do {
		digits[n++] = "0123456789abcdef"[v & 0xF];
		v >>= 4;
	} while (v != 0);
	while (n > 0) {
		*p++ = digits[--n];
	}
	return p;
}

static char *dumpDec(char *p, int v) {
	char digits[12];
	int n = 0;
	do {
		digits[n++] = '0' + v % 10;
		v /= 10;
	} while (v != 0);
	while (n > 0) {
		*p++ = digits[--n];
	}
	return p;
}

static void dumpBuffer(const u32 *buf) {
	// What's been written so far has to come out before our own lines.
	flushschedf();
	int y = 0;
	while (y < 272) {
		const u32 *row = buf + y * 512;
		int last = y;
		while (last + 1 < 272 && memcmp(buf + (last + 1) * 512, row, 480 * 4) == 0) {
			last++;
		}

		char *p = dumpLine;
		p = dumpDec(p, y);
		if (last != y) {
			*p++ = '-';
			p = dumpDec(p, last);
		}
		*p++ = ':';
		int x = 0;
		while (x < 480) {
			int end = x + 1;
			while (end < 480 && row[end] == row[x]) {
				end++;
			}
			// A group of up to 8 values repeating covers more than a plain run?
			int bestPeriod = 1, bestCount = end - x;
			for (int period = 2; period <= 8 && x + period * 2 <= 480; period++) {
				int count = 1;
				while (x + (count + 1) * period <= 480 && memcmp(row + x + count * period, row + x, period * 4) == 0) {
					count++;
				}
				if (count >= 2 && count * period > bestCount * bestPeriod) {
					bestPeriod = period;
					bestCount = count;
				}
			}
			*p++ = ' ';
			if (bestPeriod == 1) {
				p = dumpHex(p, row[x]);
			} else {
				*p++ = '(';
				for (int i = 0; i < bestPeriod; i++) {
					if (i != 0) {
						*p++ = ' ';
					}
					p = dumpHex(p, row[x + i]);
				}
				*p++ = ')';
			}
			if (bestCount > 1) {
				*p++ = '*';
				p = dumpDec(p, bestCount);
			}
			x += bestPeriod * bestCount;
		}
		*p = '\0';
		printf("%s\n", dumpLine);
		y = last + 1;
	}
}
