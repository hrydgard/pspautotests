#include <common.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>

// What sceIoChstat does to a file's mode and attributes on the memory stick.
//
// Split out of io/stat because PPSSPP doesn't implement sceIoChstat at all - it logs the request
// and returns success without touching anything, so the file never becomes read-only and the
// stat afterwards is unchanged. Implementing it means carrying a mode change down through the
// filesystem layer to the host file, which is a feature rather than a correction, so this sits on
// its own until someone does that.

#define TESTDIR "ms0:/PSP/CHSTTST"

static void statPath(const char *title, const char *path) {
	SceIoStat st;
	memset(&st, 0, sizeof(st));
	int result = sceIoGetstat(path, &st);
	if (result < 0) {
		printf("%s: sceIoGetstat = %08x\n", title, result);
		return;
	}
	printf("%s: mode=%08x attr=%04x\n", title, st.st_mode, st.st_attr);
}

int main(int argc, char **argv) {
	SceIoStat chg;

	sceIoRemove(TESTDIR "/file.txt");
	sceIoRmdir(TESTDIR);

	int mkdirResult = sceIoMkdir(TESTDIR, 0777);
	if (mkdirResult < 0) {
		printf("sceIoMkdir: %08x\n", mkdirResult);
		return 1;
	}
	int fd = sceIoOpen(TESTDIR "/file.txt", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if (fd < 0) {
		printf("create: %08x\n", fd);
		return 1;
	}
	sceIoWrite(fd, "x", 1);
	sceIoClose(fd);

	printf("-- as created:\n");
	statPath("fresh", TESTDIR "/file.txt");

	// Clearing the write bits is how a FAT file is made read-only.
	printf("-- chstat to 0444:\n");
	memset(&chg, 0, sizeof(chg));
	chg.st_mode = 0444;
	printf("sceIoChstat: %08x\n", sceIoChstat(TESTDIR "/file.txt", &chg, 0x0001));
	statPath("readonly", TESTDIR "/file.txt");

	// And whether it goes back the same way.
	printf("-- chstat back to 0777:\n");
	chg.st_mode = 0777;
	printf("sceIoChstat: %08x\n", sceIoChstat(TESTDIR "/file.txt", &chg, 0x0001));
	statPath("writable again", TESTDIR "/file.txt");

	// Setting the attribute directly rather than through the mode.
	printf("-- chstat the attribute directly:\n");
	memset(&chg, 0, sizeof(chg));
	chg.st_attr = 0x21;
	printf("sceIoChstat: %08x\n", sceIoChstat(TESTDIR "/file.txt", &chg, 0x0002));
	statPath("attr 0x21", TESTDIR "/file.txt");

	printf("-- cleanup:\n");
	chg.st_mode = 0777;
	chg.st_attr = 0x20;
	sceIoChstat(TESTDIR "/file.txt", &chg, 0x0003);
	printf("remove: %08x\n", sceIoRemove(TESTDIR "/file.txt"));
	printf("rmdir: %08x\n", sceIoRmdir(TESTDIR));
	return 0;
}
