#include <common.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>

// What does the kernel actually put in a SceIoStat on the memory stick?
//
// st_private is the interesting part: games read st_private[0] as the file's LBN (the
// disc0:/sce_lbn0x... trick, see umd/raw_access), and nothing has ever checked what the other
// five words hold, or whether the kernel writes them at all. So poison the struct first and
// report exactly which bytes came back changed.
//
// The other question is st_mode/st_attr for a plain file on a FAT volume, which no test covers -
// io/directory only checks the mode of a directory, ms0:/PSP/SAVEDATA. And whether sceIoGetstat
// and sceIoDread agree about the same file, which they need not.

#define TESTDIR "ms0:/PSP/STATTST"

#define POISON 0xCC
// Room for the struct plus a margin either side, so we notice a write past the end.
#define MARGIN 16
#define STAT_SIZE ((int)sizeof(SceIoStat))

static char scratch[MARGIN + 256 + MARGIN];

static SceIoStat *poisonedStat() {
	memset(scratch, POISON, sizeof(scratch));
	return (SceIoStat *)(scratch + MARGIN);
}

// The dates move every run, so never print them - just say whether they were written at all.
static int dateWritten(const ScePspDateTime *dt) {
	const unsigned char *p = (const unsigned char *)dt;
	int i;
	for (i = 0; i < (int)sizeof(ScePspDateTime); i++) {
		if (p[i] != POISON) {
			return 1;
		}
	}
	return 0;
}

// How far into the struct did the kernel write? Poison bytes that came back untouched tell us
// the kernel left them alone, which is what we actually want to know about st_private.
static void reportExtent(const char *title) {
	const unsigned char *p = (const unsigned char *)scratch;
	int total = sizeof(scratch);
	int first = -1, last = -1;
	int i;
	for (i = 0; i < total; i++) {
		if (p[i] != POISON) {
			if (first < 0) {
				first = i;
			}
			last = i;
		}
	}
	if (first < 0) {
		printf("%s: nothing written\n", title);
	} else {
		printf("%s: wrote [%d..%d] of the %d byte struct (margin starts at %d)\n",
			title, first - MARGIN, last - MARGIN, STAT_SIZE, MARGIN);
	}
}

static void dumpStat(const char *title, const SceIoStat *st) {
	int i;
	printf("%s: mode=%08x attr=%04x size=%d\n", title, st->st_mode, st->st_attr, (int)st->st_size);
	printf("%s: ctime/atime/mtime written = %d/%d/%d\n", title,
		dateWritten(&st->sce_st_ctime), dateWritten(&st->sce_st_atime), dateWritten(&st->sce_st_mtime));
	// Printed raw: if the kernel doesn't write st_private, these come back as the poison we
	// put there, which is just as reproducible as a real value would be.
	printf("%s: private=", title);
	for (i = 0; i < 6; i++) {
		printf("%08x%s", st->st_private[i], i == 5 ? "\n" : " ");
	}
}

static void statPath(const char *title, const char *path) {
	SceIoStat *st = poisonedStat();
	int result = sceIoGetstat(path, st);
	printf("%s: sceIoGetstat = %08x\n", title, result);
	if (result < 0) {
		return;
	}
	dumpStat(title, st);
	reportExtent(title);
}

static void createFile(const char *path, int size) {
	static char buf[256];
	int fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if (fd < 0) {
		printf("create '%s': %08x\n", path, fd);
		return;
	}
	if (size > 0) {
		memset(buf, 'x', sizeof(buf));
		while (size > 0) {
			int chunk = size > (int)sizeof(buf) ? (int)sizeof(buf) : size;
			sceIoWrite(fd, buf, chunk);
			size -= chunk;
		}
	}
	sceIoClose(fd);
}

// sceIoDread fills in a d_stat for each entry. Does it agree with sceIoGetstat for the same file?
static void compareWithDread(const char *dir, const char *name) {
	char path[256];
	SceIoStat viaGetstat;
	SceIoDirent entry;
	int fd, found = 0;

	sprintf(path, "%s/%s", dir, name);
	memset(&viaGetstat, 0, sizeof(viaGetstat));
	if (sceIoGetstat(path, &viaGetstat) < 0) {
		printf("dread vs getstat: sceIoGetstat failed\n");
		return;
	}

	fd = sceIoDopen(dir);
	if (fd < 0) {
		printf("dread vs getstat: sceIoDopen = %08x\n", fd);
		return;
	}
	while (sceIoDread(fd, &entry) > 0) {
		if (strcmp(entry.d_name, name) != 0) {
			memset(&entry, 0, sizeof(entry));
			continue;
		}
		found = 1;
		printf("dread vs getstat for '%s': mode %08x/%08x %s, attr %04x/%04x %s\n", name,
			entry.d_stat.st_mode, viaGetstat.st_mode,
			entry.d_stat.st_mode == viaGetstat.st_mode ? "same" : "DIFFER",
			entry.d_stat.st_attr, viaGetstat.st_attr,
			entry.d_stat.st_attr == viaGetstat.st_attr ? "same" : "DIFFER");
		break;
	}
	sceIoDclose(fd);
	if (!found) {
		printf("dread vs getstat: '%s' not found in listing\n", name);
	}
}

int main(int argc, char **argv) {
	sceIoRemove(TESTDIR "/plain.txt");
	sceIoRemove(TESTDIR "/readonly.txt");
	sceIoRmdir(TESTDIR "/subdir");
	sceIoRmdir(TESTDIR);

	printf("sizeof(SceIoStat) = %d\n", STAT_SIZE);

	int mkdirResult = sceIoMkdir(TESTDIR, 0777);
	if (mkdirResult < 0) {
		printf("sceIoMkdir: %08x\n", mkdirResult);
		return 1;
	}
	createFile(TESTDIR "/plain.txt", 300);
	createFile(TESTDIR "/readonly.txt", 8);
	sceIoMkdir(TESTDIR "/subdir", 0777);

	printf("-- file:\n");
	statPath("file", TESTDIR "/plain.txt");

	printf("-- empty-ish file:\n");
	statPath("small", TESTDIR "/readonly.txt");

	printf("-- directory:\n");
	statPath("dir", TESTDIR "/subdir");

	printf("-- the volume root:\n");
	statPath("ms0", "ms0:/");

	// Clearing the write bits is how you make a FAT file read-only. Does st_mode follow, and
	// does the executable bit the emulator adds for FAT show up here at all?
	printf("-- after chstat to read-only:\n");
	SceIoStat chg;
	memset(&chg, 0, sizeof(chg));
	chg.st_mode = 0444;
	printf("sceIoChstat: %08x\n", sceIoChstat(TESTDIR "/readonly.txt", &chg, 0x0001));
	statPath("readonly", TESTDIR "/readonly.txt");

	printf("-- getstat vs dread:\n");
	compareWithDread(TESTDIR, "plain.txt");
	compareWithDread(TESTDIR, "subdir");

	printf("-- cleanup:\n");
	chg.st_mode = 0777;
	sceIoChstat(TESTDIR "/readonly.txt", &chg, 0x0001);
	printf("remove plain: %08x\n", sceIoRemove(TESTDIR "/plain.txt"));
	printf("remove readonly: %08x\n", sceIoRemove(TESTDIR "/readonly.txt"));
	printf("rmdir subdir: %08x\n", sceIoRmdir(TESTDIR "/subdir"));
	printf("rmdir: %08x\n", sceIoRmdir(TESTDIR));

	return 0;
}
