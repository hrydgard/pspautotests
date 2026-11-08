// sceKernelQueryModuleInfo: which info sizes it accepts before and after the SDK version is set,
// how much of the struct it writes for each, and what it checks first.

#include <common.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <pspkernel.h>
#include <pspmodulemgr.h>
#include <string.h>
#include <sysmem-imports.h>

typedef struct {
	SceSize size;
	char nsegment;
	char reserved[3];
	int segmentaddr[4];
	int segmentsize[4];
	unsigned int entry_addr;
	unsigned int gp_value;
	unsigned int text_addr;
	unsigned int text_size;
	unsigned int data_size;
	unsigned int bss_size;
	unsigned short attribute;
	unsigned char version[2];
	char name[28];
	char padding[0x40];
} Info;

static char childPath[MAXPATHLEN];

static void testQuery(const char *title, SceUID mod, SceSize size, int useNull) {
	Info info;
	memset(&info, 0xEE, sizeof(info));
	info.size = size;
	int result = sceKernelQueryModuleInfo(mod, useNull ? NULL : (SceKernelModuleInfo *)&info);

	// The last byte that changed, so a short write and an untouched struct both show.
	int last = -1;
	const unsigned char *p = (const unsigned char *)&info;
	int i;
	for (i = 4; i < (int)sizeof(info); ++i) {
		if (p[i] != 0xEE) {
			last = i;
		}
	}
	printf("  %s: %08x, written up to %d", title, result, last);
	if (last >= 0) {
		printf(", nsegment=%d entry=%08x", info.nsegment, info.entry_addr);
	}
	if (last >= 0x40) {
		printf(", attr=%04x version=%d.%d name=%s", info.attribute, info.version[1], info.version[0], info.name);
	}
	printf("\n");
}

static void testSizes(SceUID mod) {
	testQuery("Size 0", mod, 0, 0);
	testQuery("Size 0x40", mod, 0x40, 0);
	testQuery("Size 0x48", mod, 0x48, 0);
	testQuery("Size 0x60", mod, 0x60, 0);
	testQuery("Size 0x80", mod, 0x80, 0);
	testQuery("NULL info", mod, 0x60, 1);
	testQuery("Invalid id", 0x12345678, 0x60, 0);
	testQuery("Invalid id, NULL info", 0x12345678, 0x60, 1);
}

int main(int argc, char *argv[]) {
	getcwd(childPath, MAXPATHLEN);
	strcat(childPath, "/child.prx");

	SceUID mod = sceKernelLoadModule(childPath, 0, NULL);
	if (mod < 0) {
		printf("load failed: %08x\n", mod);
		return 0;
	}

	printf("Before setting an SDK version:\n");
	testSizes(mod);

	sceKernelSetCompiledSdkVersion606(0x06060010);
	printf("SDK 6.06:\n");
	testSizes(mod);

	sceKernelUnloadModule(mod);
	return 0;
}
