// The memory stick's raw device, msstor0p1:, as the VSH's Saved Data Utility uses it: two ioctls
// on an open handle (0x02125008/9) and a devctl (0x02125802, the stick's type).
#include <common.h>

#include <pspiofilemgr.h>

static void testIoctl(const char *path, int cmd) {
	SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
	if (fd < 0) {
		checkpoint("%s ioctl %08x: open %08x", path, cmd, fd);
		return;
	}
	u32 out = 0xCCCCCCCC;
	int result = sceIoIoctl(fd, cmd, NULL, 0, &out, 4);
	checkpoint("%s ioctl %08x: %08x, out %08x", path, cmd, result, out);
	result = sceIoIoctl(fd, cmd, NULL, 0, &out, 3);
	checkpoint("%s ioctl %08x, 3 byte out: %08x", path, cmd, result);
	sceIoClose(fd);
}

static void testDevctl(const char *dev, int cmd) {
	u32 out = 0xCCCCCCCC;
	int result = sceIoDevctl(dev, cmd, NULL, 0, &out, 4);
	checkpoint("%s devctl %08x: %08x, out %08x", dev, cmd, result, out);
}

int main(int argc, char *argv[]) {
	checkpointNext("Ioctls:");
	testIoctl("msstor0p1:/", 0x02125008);
	testIoctl("msstor0p1:/", 0x02125009);
	testIoctl("msstor0p1:", 0x02125008);
	testIoctl("msstor:/", 0x02125008);

	checkpointNext("Devctls:");
	testDevctl("msstor0p1:/", 0x02125802);
	testDevctl("msstor0p1:", 0x02125802);
	testDevctl("msstor:", 0x02125802);
	testDevctl("msstor0p1:/", 0x02125801);
	return 0;
}
