#include "../partition_sweep.h"

// The same sweep as tests/sysmem/partitions, from a kernel module. See partition_sweep.h.

int main(int argc, char *argv[]) {
	sweepAllPartitions("Partitions from a kernel module:");
	return 0;
}
