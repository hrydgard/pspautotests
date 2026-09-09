#include <common.h>
#include <sysmem-imports.h>

#include <pspkernel.h>
#include <pspiofilemgr.h>

// Tests the FAT 8.3 "short name" that sceIoDread reports through d_private, and whether
// files can then be opened by it. Short names only exist on the memory stick (a real FAT
// volume), so we create a scratch directory there - host0: goes through psplink and has
// no short names at all.
//
// The layout of the d_private block is not the one in pspiofilemgr_dirent.h, and it depends
// on the compiled SDK version, so we don't decode it - we dump it and let the .expected say
// what the hardware really writes.
//
// Note for anyone running this against PPSSPP: the d_name column will not match, and that is
// deliberate on the emulator's side rather than a bug to go and fix. SimulateVFATBug() in
// DirectoryFileSystem.cpp uppercases lowercase 8.3 names on purpose, because some homebrew
// depends on that firmware behaviour for files created on a PC. Files created on the PSP - which
// is what this test does - keep their case on hardware, and its own comment says as much. The
// short names in d_private are the part an emulator can and does match.

#define TESTDIR "ms0:/PSP/SHORTTST"

// Big enough for the largest documented layout (4 + 16 + 1024).
#define PRIVATE_SIZE 1044
// How much of it to show. The interesting part is the two names at the front.
#define DUMP_SIZE 44

static const char *createNames[] = {
	// Fits in 8.3 already, lowercase.
	"readme.txt",
	// Fits in 8.3 and is already uppercase - no long name entry is needed at all.
	"UPPER.TXT",
	// Mixed case, still fits in 8.3.
	"MiXeD.txt",
	// Too long, so we expect LONGFI~1.TXT and friends. Creation order decides the number.
	"LongFileName.txt",
	"LongFileName2.txt",
	"LongFileNameThree.txt",
	// More than one dot - FAT keeps the last one as the extension.
	"a.b.c.txt",
	// No extension at all, long and short.
	"noextensionhere",
	"shrt",
	// Spaces are stripped rather than replaced.
	"sp ace.txt",
	// '+' and '[' are not valid in a short name.
	"+plus[brack].txt",
	// Leading dot, i.e. what other systems would call a hidden file.
	".hidden",
	// Extension longer than three characters.
	"toolongextension.mpeg",
};

static void createFile(const char *dir, const char *name) {
	char path[256];
	sprintf(path, "%s/%s", dir, name);
	int fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if (fd < 0) {
		printf("create '%s': %08x\n", name, fd);
		return;
	}
	sceIoWrite(fd, "x", 1);
	sceIoClose(fd);
}

static void removeFile(const char *dir, const char *name) {
	char path[256];
	sprintf(path, "%s/%s", dir, name);
	sceIoRemove(path);
}

// Renders the raw block so the .expected records the layout instead of assuming one.
// NUL shows as '|' - forbidden in a FAT name, so it can't be confused with real content -
// and anything else unprintable as '?'.
static void formatPrivate(const unsigned char *p, char *out) {
	int i;
	for (i = 0; i < DUMP_SIZE; i++) {
		unsigned char c = p[i];
		if (c == 0) {
			out[i] = '|';
		} else if (c >= 0x20 && c < 0x7F) {
			out[i] = (char)c;
		} else {
			out[i] = '?';
		}
	}
	out[DUMP_SIZE] = '\0';
}

typedef struct {
	char name[256];
	char dump[DUMP_SIZE + 1];
	unsigned int head;
} Entry;

static int compareEntry(const void *a, const void *b) {
	return strcmp(((const Entry *)a)->name, ((const Entry *)b)->name);
}

static void listShortNames(const char *dir) {
	static Entry entries[32];
	int count = 0;
	int last = 0;
	int i;

	int fd = sceIoDopen(dir);
	if (fd < 0) {
		printf("sceIoDopen('%s'): %08x\n", dir, fd);
		return;
	}

	for (;;) {
		// Aligned so the leading u32 "size" field, if the firmware wants one, is aligned too.
		unsigned int privBuf[PRIVATE_SIZE / sizeof(unsigned int)];
		unsigned char *priv = (unsigned char *)privBuf;
		SceIoDirent entry;

		memset(priv, 0, PRIVATE_SIZE);
		memset(&entry, 0, sizeof(entry));
		// Some firmwares expect the caller to declare the size up front.
		privBuf[0] = PRIVATE_SIZE;
		entry.d_private = (SceIoFatDirentPrivate *)priv;

		int result = sceIoDread(fd, &entry);
		if (result <= 0) {
			last = result;
			break;
		}

		if (count < ARRAY_SIZE(entries)) {
			strcpy(entries[count].name, entry.d_name);
			formatPrivate(priv, entries[count].dump);
			entries[count].head = privBuf[0];
			count++;
		}
	}

	sceIoDclose(fd);

	// A real FAT directory comes back in creation order, a host directory in whatever order
	// the host feels like. That's not what this test is about, so sort it away.
	qsort(entries, count, sizeof(Entry), &compareEntry);

	for (i = 0; i < count; i++) {
		// The leading word is the size field in the newer layout, and the first four
		// characters of the short name in the older one, so show it both ways.
		printf("d_name='%s' head=%08x priv='%s'\n", entries[i].name, entries[i].head, entries[i].dump);
	}
	printf("sceIoDread at end: %08x\n", last);
}

// The point of the short name is that a game can turn around and open the file by it.
static void openByName(const char *dir, const char *name) {
	char path[256];
	sprintf(path, "%s/%s", dir, name);
	int fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
	if (fd < 0) {
		printf("open '%s': %08x\n", name, fd);
	} else {
		printf("open '%s': OK\n", name);
		sceIoClose(fd);
	}
}

static void createTestFiles() {
	int i;
	for (i = 0; i < ARRAY_SIZE(createNames); i++) {
		createFile(TESTDIR, createNames[i]);
	}
	// Directories get short names too.
	int subdirResult = sceIoMkdir(TESTDIR "/LongDirectoryName", 0777);
	if (subdirResult < 0) {
		printf("sceIoMkdir subdir: %08x\n", subdirResult);
	}
}

static void removeTestFiles() {
	int i;
	for (i = 0; i < ARRAY_SIZE(createNames); i++) {
		removeFile(TESTDIR, createNames[i]);
	}
	sceIoRmdir(TESTDIR "/LongDirectoryName");
}

int main(int argc, char **argv) {
	// Leftovers from an aborted run would shift the ~1/~2 numbering, so clean up first.
	removeTestFiles();
	sceIoRmdir(TESTDIR);

	int mkdirResult = sceIoMkdir(TESTDIR, 0777);
	if (mkdirResult < 0) {
		printf("sceIoMkdir: %08x\n", mkdirResult);
		return 1;
	}
	createTestFiles();

	printf("-- listing (sdkver as launched):\n");
	listShortNames(TESTDIR);

	// The block layout is documented as changing at SDK 3.08, so check both sides of that.
	printf("-- listing (sdkver 3.07):\n");
	sceKernelSetCompiledSdkVersion(0x03070110);
	listShortNames(TESTDIR);

	printf("-- listing (sdkver 6.06):\n");
	sceKernelSetCompiledSdkVersion606(0x06060010);
	listShortNames(TESTDIR);

	printf("-- open by short name:\n");
	openByName(TESTDIR, "LONGFI~1.TXT");
	openByName(TESTDIR, "longfi~1.txt");
	openByName(TESTDIR, "README.TXT");
	openByName(TESTDIR, "NOEXTE~1");
	// A short name we never handed out, to make sure they aren't invented on the fly.
	openByName(TESTDIR, "NOSUCH~1.TXT");

	printf("-- cleanup:\n");
	removeTestFiles();
	printf("sceIoRmdir: %08x\n", sceIoRmdir(TESTDIR));

	return 0;
}
