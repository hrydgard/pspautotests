#include <common.h>

#include <pspiofilemgr.h>
#include <string.h>

// Basic file calls on ms0:, in a directory of the test's own, and what they return when they fail.

#define WORKDIR "ms0:/__filetest"

static const char *fileText = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
static char readBuf[128];

static void section(const char *title) {
	schedf("\n%s\n", title);
}

static void doRead(SceUID fd, int size) {
	int ret;
	memset(readBuf, 0, sizeof(readBuf));
	ret = sceIoRead(fd, readBuf, size);
	if (ret > 0) {
		schedf("  Read %d: %d = %s\n", size, ret, readBuf);
	} else {
		schedf("  Read %d: %08x\n", size, ret);
	}
}

static void doSeek(SceUID fd, SceOff offset, int whence) {
	SceOff ret = sceIoLseek(fd, offset, whence);
	schedf("  Lseek %lld, %d: %08x%08x\n", offset, whence, (unsigned)(ret >> 32), (unsigned)ret);
}

static void doSeek32(SceUID fd, int offset, int whence) {
	schedf("  Lseek32 %d, %d: %08x\n", offset, whence, sceIoLseek32(fd, offset, whence));
}

static void printStat(const char *name) {
	SceIoStat st;
	int ret;
	memset(&st, 0, sizeof(st));
	ret = sceIoGetstat(name, &st);
	if (ret < 0) {
		schedf("  Getstat %s: %08x\n", name, ret);
	} else {
		schedf("  Getstat %s: attr=%04x mode=%04x size=%lld\n", name, st.st_attr, st.st_mode, st.st_size);
	}
}

static void writeFile(const char *name) {
	SceUID fd = sceIoOpen(name, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	sceIoWrite(fd, fileText, strlen(fileText));
	sceIoClose(fd);
}

static void cleanup() {
	sceIoRemove(WORKDIR "/testfile");
	sceIoRemove(WORKDIR "/testfile2");
	sceIoRemove(WORKDIR "/other");
	sceIoRemove(WORKDIR "/sub/inner");
	sceIoRmdir(WORKDIR "/sub");
	sceIoRmdir(WORKDIR);
}

int main(int argc, char **argv) {
	SceUID fd;

	cleanup();
	schedf("Mkdir: %08x\n", sceIoMkdir(WORKDIR, 0777));
	schedf("Chdir: %08x\n", sceIoChdir(WORKDIR));

	section("Missing files:");
	printStat("nofile");
	schedf("  Open: %08x\n", sceIoOpen("nofile", PSP_O_RDONLY, 0777));
	schedf("  Close 9999: %08x\n", sceIoClose(9999));
	schedf("  Open a directory: %s\n", (fd = sceIoOpen(".", PSP_O_RDONLY, 0777)) >= 0 ? "fd" : "error");
	if (fd >= 0) {
		sceIoClose(fd);
	}

	section("Write only:");
	fd = sceIoOpen("testfile", PSP_O_WRONLY | PSP_O_CREAT, 0777);
	schedf("  Open: %s\n", fd >= 0 ? "fd" : "error");
	schedf("  Write: %d\n", sceIoWrite(fd, fileText, strlen(fileText)));
	printStat("testfile");
	doRead(fd, 128);
	schedf("  Write 0 bytes: %08x\n", sceIoWrite(fd, fileText, 0));
	schedf("  Write NULL, 0 bytes: %08x\n", sceIoWrite(fd, NULL, 0));
	schedf("  Write NULL: %08x\n", sceIoWrite(fd, NULL, 16));
	schedf("  Write from kernel memory: %08x\n", sceIoWrite(fd, (void *)0x88000000, 16));
	schedf("  Write negative size: %08x\n", sceIoWrite(fd, fileText, -1));
	printStat("testfile");
	schedf("  Close: %08x\n", sceIoClose(fd));

	section("Seeking:");
	fd = sceIoOpen("testfile", PSP_O_RDONLY, 0777);
	doRead(fd, 128);
	doSeek(fd, -10, PSP_SEEK_END);
	doRead(fd, 128);
	doSeek(fd, -10, PSP_SEEK_CUR);
	doRead(fd, 5);
	doSeek(fd, -10, PSP_SEEK_CUR);
	doRead(fd, 128);
	doSeek(fd, -10, PSP_SEEK_SET);
	doSeek(fd, 200, PSP_SEEK_SET);
	doRead(fd, 128);
	doSeek(fd, -10, PSP_SEEK_CUR);
	doRead(fd, 128);
	doSeek(fd, 0, PSP_SEEK_END);
	doSeek(fd, -100, PSP_SEEK_END);
	doSeek(fd, -100, PSP_SEEK_CUR);
	doSeek(fd, 0, 3);
	doSeek(fd, 0, -1);
	doSeek(fd, 0x100000000LL, PSP_SEEK_SET);
	doSeek(fd, 0, PSP_SEEK_CUR);
	doSeek(fd, 0x7FFFFFFFLL, PSP_SEEK_SET);
	doSeek(fd, 0x80000000LL, PSP_SEEK_SET);
	doSeek(fd, 0, PSP_SEEK_SET);
	doSeek32(fd, -10, PSP_SEEK_END);
	doRead(fd, 128);
	doSeek32(fd, -10, PSP_SEEK_CUR);
	doRead(fd, 5);
	doSeek32(fd, -10, PSP_SEEK_SET);
	doSeek32(fd, 0, 3);
	doSeek32(fd, 0x7FFFFFFF, PSP_SEEK_SET);
	doSeek32(fd, 0, PSP_SEEK_SET);
	schedf("  Write: %08x\n", sceIoWrite(fd, fileText, strlen(fileText)));
	schedf("  Close: %08x\n", sceIoClose(fd));
	doSeek(fd, 0, PSP_SEEK_SET);

	section("Append:");
	fd = sceIoOpen("testfile", PSP_O_RDWR | PSP_O_APPEND, 0777);
	schedf("  Write: %d\n", sceIoWrite(fd, fileText, strlen(fileText)));
	doSeek32(fd, 0, PSP_SEEK_SET);
	schedf("  Write after seek: %d\n", sceIoWrite(fd, "ab", 2));
	doSeek32(fd, 0, PSP_SEEK_CUR);
	doSeek32(fd, 0, PSP_SEEK_SET);
	doRead(fd, 128);
	schedf("  Close: %08x\n", sceIoClose(fd));

	section("Rename:");
	schedf("  Rename: %08x\n", sceIoRename("testfile", "testfile2"));
	printStat("testfile");
	printStat("testfile2");
	schedf("  Missing file: %08x\n", sceIoRename("nofile", "nofile2"));
	writeFile("other");
	schedf("  Onto a file: %08x\n", sceIoRename("other", "testfile2"));
	schedf("  Into a missing dir: %08x\n", sceIoRename("other", "nodir/other"));
	schedf("  Onto itself: %08x\n", sceIoRename("other", "other"));

	section("Mkdir:");
	schedf("  New: %08x\n", sceIoMkdir("sub", 0777));
	schedf("  Existing dir: %08x\n", sceIoMkdir("sub", 0777));
	schedf("  Existing file: %08x\n", sceIoMkdir("other", 0777));
	schedf("  Missing parent: %08x\n", sceIoMkdir("nodir/sub", 0777));
	schedf("  Root: %08x\n", sceIoMkdir("ms0:/", 0777));
	schedf("  Bad device: %08x\n", sceIoMkdir("nodev0:/sub", 0777));
	printStat("sub");

	section("Remove:");
	writeFile("sub/inner");
	schedf("  File: %08x\n", sceIoRemove("other"));
	schedf("  Again: %08x\n", sceIoRemove("other"));
	schedf("  A directory: %08x\n", sceIoRemove("sub"));
	schedf("  Missing parent: %08x\n", sceIoRemove("nodir/file"));
	fd = sceIoOpen("testfile2", PSP_O_RDONLY, 0777);
	schedf("  An open file: %08x\n", sceIoRemove("testfile2"));
	printStat("testfile2");
	doRead(fd, 4);
	sceIoClose(fd);

	section("Rmdir:");
	schedf("  Not empty: %08x\n", sceIoRmdir("sub"));
	schedf("  A file: %08x\n", sceIoRmdir("sub/inner"));
	schedf("  Missing: %08x\n", sceIoRmdir("nodir"));
	sceIoRemove("sub/inner");
	schedf("  Empty: %08x\n", sceIoRmdir("sub"));
	schedf("  Again: %08x\n", sceIoRmdir("sub"));
	schedf("  Bad device: %08x\n", sceIoRmdir("nodev0:/sub"));

	sceIoChdir("ms0:/");
	cleanup();
	printStat(WORKDIR);
	flushschedf();
	return 0;
}
