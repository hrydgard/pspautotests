// Memory stick driver devctls the XMB uses, besides the stat/free space ones:
// - ms0: 0x02415830, a rename that takes the new name as a full path (fatms.prx).
// - mscmhc0: 0x0201580B, stores driver parameters (msstor.prx). Only called with an empty mask
//   here, so nothing is changed.
// - fatms0: 0x0240D81E, invalidates the driver's file table cache.
// The UTF-16 path devctls (0x02415860-0x02415865, mkdir among them) are gone from 6.61's fatms.prx.
#include <common.h>

#include <pspiofilemgr.h>

#define DIR "ms0:/PSP/__FATDEV"
// The rename devctl's paths are on the device, without the "ms0:".
#define REL "/PSP/__FATDEV"

static const char *const files[] = {
	DIR "/f.txt", DIR "/e.txt", DIR "/n.txt", DIR "/sub/g.txt", DIR "/sub/e.txt", DIR "/sub2/g.txt",
	DIR "/sub2/e.txt", DIR "/r.txt", DIR "/q.txt", DIR "/sub/q.txt", DIR "/sub2/q.txt", DIR "/fat.txt",
};
static const char *const dirs[] = { DIR "/sub", DIR "/sub2", DIR };

static void cleanup() {
	for (size_t i = 0; i < ARRAY_SIZE(files); ++i) {
		sceIoRemove(files[i]);
	}
	for (size_t i = 0; i < ARRAY_SIZE(dirs); ++i) {
		sceIoRmdir(dirs[i]);
	}
}

static void makeFile(const char *path) {
	SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if (fd < 0) {
		printf("create %s: %08x\n", path, fd);
		return;
	}
	sceIoWrite(fd, "hello", 5);
	sceIoClose(fd);
}

static void showExists(const char *path) {
	SceIoStat st;
	int result = sceIoGetstat(path, &st);
	printf("  %s: %s\n", path, result < 0 ? "missing" : (FIO_S_ISDIR(st.st_mode) ? "dir" : "file"));
}

static int moveDevctl(const char *dev, const char *from, const char *to, int len) {
	const char *args[2] = { from, to };
	return sceIoDevctl(dev, 0x02415830, args, len, NULL, 0);
}

static void testMove(const char *title, const char *from, const char *to) {
	printf("%s: %08x\n", title, moveDevctl("ms0:", from, to, 8));
}

int main(int argc, char *argv[]) {
	cleanup();
	sceIoMkdir(DIR, 0777);
	sceIoMkdir(DIR "/sub", 0777);
	makeFile(DIR "/f.txt");
	makeFile(DIR "/e.txt");
	makeFile(DIR "/sub/e.txt");

	printf("ms0: 0x02415830\n");
	testMove("To another directory", REL "/f.txt", REL "/sub/g.txt");
	showExists(DIR "/f.txt");
	showExists(DIR "/sub/g.txt");
	testMove("Onto an existing file", REL "/sub/g.txt", REL "/sub/e.txt");
	showExists(DIR "/sub/g.txt");
	testMove("Missing source", REL "/n.txt", REL "/m.txt");
	testMove("A directory", REL "/sub", REL "/sub2");
	showExists(DIR "/sub");
	showExists(DIR "/sub2/g.txt");
	testMove("No leading slash", "PSP/__FATDEV/e.txt", "PSP/__FATDEV/r.txt");
	showExists(DIR "/e.txt");
	showExists(DIR "/r.txt");
	testMove("Same name", REL "/r.txt", REL "/r.txt");
	testMove("With the device name", "ms0:" REL "/r.txt", "ms0:" REL "/q.txt");
	showExists(DIR "/r.txt");
	showExists(DIR "/q.txt");

	const char *nullFrom[2] = { NULL, REL "/x.txt" };
	const char *nullTo[2] = { REL "/r.txt", NULL };
	printf("4 bytes: %08x\n", moveDevctl("ms0:", REL "/r.txt", REL "/q.txt", 4));
	printf("NULL argument: %08x\n", sceIoDevctl("ms0:", 0x02415830, NULL, 8, NULL, 0));
	printf("NULL old name: %08x\n", sceIoDevctl("ms0:", 0x02415830, nullFrom, 8, NULL, 0));
	printf("NULL new name: %08x\n", sceIoDevctl("ms0:", 0x02415830, nullTo, 8, NULL, 0));

	makeFile(DIR "/fat.txt");
	printf("fatms0: %08x\n", moveDevctl("fatms0:", REL "/fat.txt", REL "/sub2/q.txt", 8));
	showExists(DIR "/fat.txt");
	showExists(DIR "/sub2/q.txt");

	// For comparison: sceIoRename given a directory in the new name.
	makeFile(DIR "/q.txt");
	printf("sceIoRename into sub2: %08x\n", sceIoRename(DIR "/q.txt", DIR "/sub2/e.txt"));
	showExists(DIR "/q.txt");
	showExists(DIR "/e.txt");
	showExists(DIR "/sub2/e.txt");

	printf("\nmscmhc0: 0x0201580B\n");
	u32 params[5] = { 0, 0, 0, 0, 0 };
	printf("Empty mask: %08x\n", sceIoDevctl("mscmhc0:", 0x0201580B, params, 20, NULL, 0));
	printf("16 bytes: %08x\n", sceIoDevctl("mscmhc0:", 0x0201580B, params, 16, NULL, 0));
	printf("NULL: %08x\n", sceIoDevctl("mscmhc0:", 0x0201580B, NULL, 20, NULL, 0));

	printf("\nfatms0: 0x0240D81E\n");
	printf("Invalidate: %08x\n", sceIoDevctl("fatms0:", 0x0240D81E, NULL, 0, NULL, 0));

	cleanup();
	return 0;
}
