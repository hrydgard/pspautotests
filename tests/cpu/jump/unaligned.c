#include <common.h>

// jr and jalr to a target with its low two bits set. MIPS raises an address error for that; does the
// Allegrex? For each offset, prints which instructions of a ladder at the (aligned) target ran (a bit
// each: +0 and +4), the link jalr writes (from its address + 8), and the link a bal at target + 8 writes
// (from target + 16) - which would carry the low bits if the PC kept them.

static void __attribute__((noinline)) jumpTest(int useJalr, int offset, int *ran, int *link, int *balLink) {
	int r, l, b, target, jalrNext;
	asm volatile (
		".set    noreorder\n"
		"move    $t2, $ra\n"
		"la      %3, target_%=\n"
		"la      %4, jalr_next_%=\n"
		"addu    $t0, %3, %5\n"
		"li      %0, 0\n"
		"li      $t1, 0\n"
		"li      $ra, 0\n"
		"beqz    %6, use_jr_%=\n"
		"nop\n"
		"jalr    $t1, $t0\n"
		"nop\n"
		"jalr_next_%=:\n"
		"li      %0, 0x100\n"
		"b       done_%=\n"
		"nop\n"
		"use_jr_%=:\n"
		"jr      $t0\n"
		"nop\n"
		"li      %0, 0x200\n"
		"b       done_%=\n"
		"nop\n"
		".balign 16\n"
		"target_%=:\n"
		"ori     %0, %0, 1\n"
		"ori     %0, %0, 2\n"
		"bal     bal_next_%=\n"
		"nop\n"
		"bal_next_%=:\n"
		"done_%=:\n"
		"move    %1, $t1\n"
		"move    %2, $ra\n"
		"move    $ra, $t2\n"
		".set    reorder\n"
		: "=&r"(r), "=&r"(l), "=&r"(b), "=&r"(target), "=&r"(jalrNext) : "r"(offset), "r"(useJalr) : "t0", "t1", "t2", "memory"
	);
	*ran = r;
	*link = l == 0 ? 0x7FFFFFFF : l - jalrNext;
	*balLink = b == 0 ? 0x7FFFFFFF : b - (target + 16);
}

int main(int argc, char **argv) {
	for (int useJalr = 0; useJalr < 2; useJalr++) {
		for (int offset = 0; offset < 4; offset++) {
			int ran, link, balLink;
			jumpTest(useJalr, offset, &ran, &link, &balLink);
			if (useJalr) {
				printf("jalr target+%d: ran=%03x link=next%+d bal ra=next%+d\n", offset, ran, link, balLink);
			} else {
				printf("jr   target+%d: ran=%03x bal ra=next%+d\n", offset, ran, balLink);
			}
			// So a crash on a later case leaves what came before.
			fflush(stdout);
		}
	}
	return 0;
}
