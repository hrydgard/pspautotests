#include <common.h>
#include <pspmodulemgr.h>
#include <pspsysmem.h>
#include <pspthreadman.h>

struct SceKernelTlsplOptParam;
extern "C" int scePowerVolatileMemLock(int, void **, int *);
extern "C" int scePowerVolatileMemUnlock(int);
extern "C" SceUID sceKernelCreateTlspl(const char *name, u32 partitionid, u32 attr, u32 blockSize, u32 count, SceKernelTlsplOptParam *options);
extern "C" int sceKernelDeleteTlspl(SceUID uid);
extern "C" void *sceKernelGetTlsAddr(SceUID uid);

void testAlloc(const char *title, const char *name, int type, u32 size, void *addr) {
	SceUID uid = sceKernelAllocPartitionMemory(5, name, type, size, addr);
	if (uid >= 0) {
		checkpoint("%s: OK: %08x", title, sceKernelGetBlockHeadAddr(uid));
		sceKernelFreePartitionMemory(uid);
	} else {
		checkpoint("%s: Failed (%08x)", title, uid);
	}
}

extern "C" int main(int argc, char *argv[]) {
	checkpointNext("Names:");
	testAlloc("  NULL", NULL, 1, 0x100, NULL);
	testAlloc("  Blank", "", 1, 0x100, NULL);
	testAlloc("  Long", "123456789012345678901234567890123456789012345678901234567890", 1, 0x100, NULL);

	checkpointNext("Types:");
	static const int types[] = {-1, 0, 1, 2, 3, 4, 5, 0x11, 0x30, 0x101, 0x1001, 0x10001, 0x100001, 0x1000001, 0x10000001};
	for (size_t i = 0; i < ARRAY_SIZE(types); ++i) {
		char temp[128];
		sprintf(temp, "  Type %x", types[i]);
		testAlloc(temp, "test", types[i], 0x100, NULL);
	}

	testAlloc("  Type 2 in range", "", 2, 0x100, (void *)0x08500000);
	testAlloc("  Type 2 outside", "", 2, 0x100, (void *)0x08900000);
	testAlloc("  Type 3 align value", "", 3, 0x100, (void *)128);
	testAlloc("  Multi-error", NULL, 0x30, 0, (void *)0x08500000);

	checkpointNext("Alignments:");
	static const int alignments[] = {-1, 1, 2, 3, 4, 6, 8, 16, 512, 1024, 2048};
	for (size_t i = 0; i < ARRAY_SIZE(alignments); ++i) {
		char temp[128];
		sprintf(temp, "  Type 4 align 0x%x", alignments[i]);
		testAlloc(temp, "test", 4, 0x100, (void *)alignments[i]);
	}

	checkpointNext("Sizes:");
	static const int sizes[] = {-1, 0, 1, 2, 3, 4, 5, 0x11, 0x101, 0x1001, 0x10001, 0x003FFF00, 0x00400000};
	for (size_t i = 0; i < ARRAY_SIZE(sizes); ++i) {
		SceUID uid1 = sceKernelAllocPartitionMemory(5, "test", 1, 0x100, NULL);
		SceUID uid2 = sceKernelAllocPartitionMemory(5, "test", 1, sizes[i], NULL);
		if (uid2 >= 0) {
			u8 *ptr1 = (u8 *)sceKernelGetBlockHeadAddr(uid1);
			u8 *ptr2 = (u8 *)sceKernelGetBlockHeadAddr(uid2);
			sceKernelFreePartitionMemory(uid2);

			checkpoint("  Size %x: OK, delta=%x, addr=%08x", sizes[i], ptr1 - ptr2, ptr2);
		} else {
			checkpoint("  Size %x: Failed (%08x)", sizes[i], uid2);
		}
		if (uid1 > 0) {
			sceKernelFreePartitionMemory(uid1);
		}
	}

	checkpointNext("Other objects:");
	void *ptr;

	SceUID fpl = sceKernelCreateFpl("test", 5, 0, 0x1000, 4, NULL);
	ptr = NULL;
	sceKernelAllocateFpl(fpl, &ptr, NULL);
	sceKernelDeleteFpl(fpl);
	checkpoint("  FPL: %08x", ptr);

	// Creating a VPL writes the header, so requires memory to be unlocked or it crashes.
	scePowerVolatileMemLock(0, NULL, NULL);

	SceUID vpl = sceKernelCreateVpl("test", 5, 0, 0x1000, NULL);
	ptr = NULL;
	sceKernelAllocateVpl(vpl, 0x100, &ptr, NULL);
	sceKernelDeleteVpl(vpl);
	checkpoint("  VPL: %08x", ptr);

	// TLSPL also requires unlocked, allocating clears.
	SceUID tlspl = sceKernelCreateTlspl("test", 5, 0, 0x100, 4, NULL);
	ptr = sceKernelGetTlsAddr(tlspl);
	sceKernelDeleteTlspl(tlspl);
	checkpoint("  TLSPL: %08x", ptr);

	u32 beforeFree = sceKernelTotalFreeMemSize();
	SceUID msgpipe = sceKernelCreateMsgPipe("test", 5, 0, (void *)0x1000, NULL);
	u32 afterFree = sceKernelTotalFreeMemSize();
	sceKernelDeleteMsgPipe(msgpipe);
	checkpoint("  Msgpipe: allocated %08x / %s", afterFree - beforeFree, msgpipe > 0 ? "OK" : "Failed");

	scePowerVolatileMemUnlock(0);

	return 0;
}