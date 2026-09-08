#include "partition_sweep.h"

// Which partitions each memory API accepts, from an ordinary user module. The kernel-mode twin
// lives in tests/sysmem/kernel/partitions - diff the two to see what privilege changes.

int main(int argc, char *argv[]) {
	sweepAllPartitions("Partitions from a user module:");
	return 0;
}
