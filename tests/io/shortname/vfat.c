#include <common.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>

// Does a name written to the memory stick by a PC keep its case when the PSP lists it?
//
// It does not, and that is what PPSSPP's SimulateVFATBug() in DirectoryFileSystem.cpp reproduces.
// Recorded on 6.61 with the files written from macOS:
//
//     short='SHRT'         d_name='SHRT'               <- created as "shrt"
//     short='README.TXT'   d_name='README.TXT'         <- created as "readme.txt"
//     short='UPPER.TXT'    d_name='UPPER.TXT'
//     short='MIXED.TXT'    d_name='MiXeD.txt'
//     short='LONGFI~7.TXT' d_name='LongFileName.txt'
//
// So a lowercase 8.3 name comes back uppercased, while anything needing a long name entry keeps
// its case exactly. That is worth writing down, because io/shortname says the opposite for files
// the PSP creates itself - there "shrt" stays "shrt" and "readme.txt" stays "readme.txt". Both are
// true. The difference is who wrote the directory entry:
//
//   - The PSP sets the FAT lowercase flag for the base of a short name, and honours it on read.
//   - macOS does not set that flag at all. It writes the plain uppercase short name, and spends a
//     long name entry whenever the name can't survive as one - which is why the mixed and long
//     names above are intact while "shrt" and "readme.txt" are not.
//
// An emulator seeing a host directory has no way to tell the two apart, since the host filename
// is all that is left. SimulateVFATBug picks the PC-written reading, which is the right guess for
// the common case of a user copying files into their memstick folder. Don't remove it on the
// strength of io/shortname alone.
//
// Two things still open, neither answerable from a Mac:
//
//   - Windows does set the lowercase flags. A stick written from Windows may well behave like the
//     PSP-written case rather than this one, and the flag for the extension is a separate bit from
//     the flag for the base - so "readme.txt" could come back as "readme.TXT". Nobody has checked.
//   - The two names that would have separated the base flag from the extension flag, MIXED.txt and
//     mixed.TXT, cannot be tested this way: macOS FAT is case insensitive, so they collapse into
//     MiXeD.txt and only five of the seven files below ever reach the stick. They would need to go
//     in separate directories.
//
// MANUAL SETUP, and not in test.py for that reason - CI cannot reproduce it, and the answer
// depends on which operating system wrote the files. Connect the PSP as USB mass storage and,
// from the PC, create ms0:/PSP/VFATTST containing empty files named:
//
//     shrt   readme.txt   UPPER.TXT   MiXeD.txt   LongFileName.txt
//
// Then disconnect, start PSPLink, and run this. The short name is printed alongside: a "~" in it
// means the writer spent a long name entry rather than relying on the flags.

#define TESTDIR "ms0:/PSP/VFATTST"
#define MAX_ENTRIES 32

typedef struct {
	char name[256];
	char shortName[16];
} Entry;

static Entry entries[MAX_ENTRIES];

// Sorted by short name rather than d_name: the short name is uppercase and stable, while d_name
// is the very thing being measured and would reorder the output depending on the answer.
static int compareEntry(const void *a, const void *b) {
	return strcmp(((const Entry *)a)->shortName, ((const Entry *)b)->shortName);
}

int main(int argc, char **argv) {
	int count = 0;
	int i;

	int fd = sceIoDopen(TESTDIR);
	if (fd < 0) {
		printf("sceIoDopen('%s'): %08x\n", TESTDIR, fd);
		printf("Set the directory up from a PC first - see the comment at the top of this file.\n");
		return 1;
	}

	for (;;) {
		// The pre-3.08 layout: short name at byte 0, long name at 13, no size field. That's what
		// a test gets when it doesn't call sceKernelSetCompiledSdkVersion - see io/shortname.
		unsigned char priv[1044];
		SceIoDirent entry;

		memset(priv, 0, sizeof(priv));
		memset(&entry, 0, sizeof(entry));
		entry.d_private = (SceIoFatDirentPrivate *)priv;

		if (sceIoDread(fd, &entry) <= 0) {
			break;
		}
		if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0) {
			continue;
		}
		if (count >= MAX_ENTRIES) {
			continue;
		}
		strcpy(entries[count].name, entry.d_name);
		priv[12] = '\0';
		strcpy(entries[count].shortName, (const char *)priv);
		count++;
	}
	sceIoDclose(fd);

	qsort(entries, count, sizeof(Entry), &compareEntry);

	printf("-- %d entries:\n", count);
	for (i = 0; i < count; i++) {
		printf("short='%s' d_name='%s'\n", entries[i].shortName, entries[i].name);
	}

	if (count == 0) {
		printf("Directory is empty - copy the seven files in from a PC, not from the PSP.\n");
	}
	return 0;
}
