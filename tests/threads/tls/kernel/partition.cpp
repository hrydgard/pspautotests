#include "../shared.h"

// The same partition sweep as threads/tls/partition, run from a kernel module.
//
// Several partitions only exist for a privileged caller, so the user-mode recording alone can't
// tell you whether a rejection means "no such partition" or "not from where you're asking".
// Diff this against threads/tls/partition.expected to see which ones the privilege buys.

extern "C" int main(int argc, char *argv[]) {
	char temp[128];
	static const int parts[] = { -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

	checkpointNext("Partitions from a kernel module:");
	for (size_t i = 0; i < ARRAY_SIZE(parts); ++i) {
		SceUID tls = sceKernelCreateTlspl("tls", parts[i], 0, 0x100, 4, NULL);
		if (tls > 0) {
			sprintf(temp, "  Partition %d: OK", parts[i]);
		} else {
			sprintf(temp, "  Partition %d: %08x", parts[i], tls);
		}
		checkpoint("%s", temp);
		if (tls > 0) {
			sceKernelDeleteTlspl(tls);
		}
	}
	return 0;
}
