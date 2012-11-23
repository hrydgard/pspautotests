#include <common.h>

#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <psptypes.h>
#include <pspiofilemgr.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <sys/stat.h>

//#include "../common/emits.h"

char buf[MAXPATHLEN] = {0};
char startPath[MAXPATHLEN] = {0};


/**
 * Utility for .
 */
void checkChangePathsTry(const char *name, const char *dest, int offset) {
	if (chdir(dest) < 0) {
		printf("%s: (chdir error)\n", name);
	} else {
		char *result = getcwd(buf, MAXPATHLEN);
		int len = strlen(buf);
		if (len <= offset) {
			printf("%s: outside or at basedir\n", name);
		} else {
			printf("%s: %s\n", name, result ? result + offset : "(getcwd error)");
		}
	}
}

/**
 * Check changing paths.
 */
void checkChangePaths() {
	char baseDir[MAXPATHLEN];
	int baseDirLen;

	printf("result: %d\n", getcwd(baseDir, MAXPATHLEN) == baseDir);
	baseDirLen = strlen(baseDir);

	// Setup paths.
	mkdir("otherdir", 0777);
	mkdir("testdir", 0777);
	mkdir("testdir/testdir2", 0777);

	checkChangePathsTry("Initial", "testdir/testdir2", baseDirLen);
	checkChangePathsTry("Empty", "", baseDirLen);
	checkChangePathsTry("Non-existent", "hello", baseDirLen);
	checkChangePathsTry("Parent", "..", baseDirLen);
	checkChangePathsTry("Parent + subdirs", "../testdir/testdir2", baseDirLen);
	checkChangePathsTry("Multiple parents", "../..", baseDirLen);
	checkChangePathsTry("Back to testdir", "testdir", baseDirLen);
	checkChangePathsTry("Current dir", ".", baseDirLen);
	checkChangePathsTry("Current + extra slashes", "./././//testdir2", baseDirLen);
	checkChangePathsTry("Switch drive no slash", "ms0:", 0);
	checkChangePathsTry("Switch drive + slash", "ms0:/", 0);
	checkChangePathsTry("Absolute + drive", "ms0:/PSP/SAVEDATA", 0);
	checkChangePathsTry("Absolute no drive", "/PSP", 0);
	checkChangePathsTry("Root", "/", 0);
	checkChangePathsTry("Flash drive", "flash0:/", 0);
	checkChangePathsTry("PSP and back again + trailing /", "ms0:/PSP/../PSP/", 0);

	chdir(startPath);
}

/**
 * Check opening files.
 */
void checkFileOpen() {
	FILE *file = fopen("io.expected", "rb");
	printf("%d\n", file != NULL);
	if (file != NULL) {
		fseek(file, 0, SEEK_END);
		printf("%d\n", ftell(file) > 0);
		fclose(file);
	} else {
		printf("%d\n", 0);
	}
}

/**
 * Check listing directories.
 */
void checkDirectoryList() {
	DIR *dir;
	struct dirent *dp;
	dir = opendir(".");
	if (dir != NULL) {
		while ((dp = readdir(dir)) != NULL) {
			if (strncmp("io", dp->d_name, 2) == 0) {
				printf("%s\n", dp->d_name);
			}
		}
		closedir(dir);
	}
}

/**
 * Check listing directories extended.
 * Check that ms0:/PSP contains a directory called GAME with st_attr containing ONLY FIO_SO_IFDIR flag.
 */
void checkDirectoryListEx() {
	#define MAX_ENTRY 16
	SceIoDirent files[MAX_ENTRY];
	int fd, nfiles = 0;

	fd = sceIoDopen("ms0:/PSP");
	while (nfiles < MAX_ENTRY) {
		memset(&files[nfiles], 0x00, sizeof(SceIoDirent));
		if (sceIoDread(fd, &files[nfiles]) <= 0) break;
		if (files[nfiles].d_name[0] == '.') continue;

		if (strcmp(files[nfiles].d_name, "GAME") == 0) {
			// st_attr should only contain FIO_SO_IFDIR flag. (Required by NesterJ).
			if (files[nfiles].d_stat.st_attr == FIO_SO_IFDIR){
				printf("%s\n", files[nfiles].d_name);
				continue;
			}
		}
	}
	sceIoDclose(fd);
}

/**
 * Check that there are arguments passed to the app.
 * And the first argument should contain the path to the executable.
 * That path will be used to determine current working directory in libc/newlib.
 */
void checkMainArgs(int argc, char** argv) {
	int n;
	printf("%d\n", argc);

	if (strlen(argv[0]) > strlen(startPath)) {
		printf("%s\n", argv[0] + strlen(startPath));
	} else {
		printf("%s\n", argv[0]);
	}

	for (n = 1; n < argc; n++) {
		printf("%s\n", argv[n]);
	}
}

int main(int argc, char** argv) {
	getcwd(startPath, MAXPATHLEN);
	checkMainArgs(argc, argv);
	checkChangePaths();
	checkFileOpen();
	checkDirectoryList();
	checkDirectoryListEx();
	return 0;
}