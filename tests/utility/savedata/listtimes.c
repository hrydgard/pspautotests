#include "shared.h"

// What the timestamps in a LIST entry are, and which of them move when the save is read or
// rewritten. Crush compares a whole LIST entry with one it took after saving, and warns that the
// Memory Stick may have changed when they differ.

static char savedata[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6
};
static char readbuf[64];

static int runQuiet(SceUtilitySavedataParam2 *param) {
	int result = sceUtilitySavedataInitStart((SceUtilitySavedataParam *)param);
	if (result != 0) {
		return result;
	}
	int i;
	for (i = 0; i < 100000; ++i) {
		int status = sceUtilitySavedataGetStatus();
		if (status == 2) {
			sceUtilitySavedataUpdate(1);
		} else if (status == 3) {
			sceUtilitySavedataShutdownStart();
		} else if (status == 0 && i > 0) {
			break;
		}
		sceKernelDelayThread(2000);
	}
	return param->base.result;
}

static void initParams(SceUtilitySavedataParam2 *param, int mode) {
	initStandardSavedataParams(param);
	param->mode = (PspUtilitySavedataMode)mode;
	param->dataBuf = savedata;
	param->dataBufSize = sizeof(savedata);
	param->dataSize = sizeof(savedata);
}

static int listEntry(SceUtilitySavedataIdListEntry2 *entry) {
	SceUtilitySavedataParam2 param;
	SceUtilitySavedataIdListInfo2 idList;
	SceUtilitySavedataIdListEntry2 entries[4];
	initParams(&param, SCE_UTILITY_SAVEDATA_TYPE_LIST);
	memset(entries, 0, sizeof(entries));
	idList.maxCount = 4;
	idList.resultCount = -1;
	idList.entries = entries;
	param.idList = &idList;
	int result = runQuiet(&param);
	memcpy(entry, &entries[0], sizeof(*entry));
	checkpoint("  LIST: %08x, %d entries, mode=%08x name=%s", result, idList.resultCount, entries[0].st_mode, entries[0].name);
	return result;
}

static int sameTime(const ScePspDateTime *a, const ScePspDateTime *b) {
	return memcmp(a, b, sizeof(*a)) == 0;
}

static int timeOfDayZero(const ScePspDateTime *t) {
	return t->hour == 0 && t->minute == 0 && t->second == 0 && t->microsecond == 0;
}

static void describeTime(const char *title, const ScePspDateTime *t) {
	checkpoint("    %s: year ok=%d, microsecond zero=%d", title, t->year >= 2000, t->microsecond == 0);
}

// Only what can't come out differently between runs: a ctime with an even second, or one equal to
// mtime, can be chance.
static void describeEntry(const SceUtilitySavedataIdListEntry2 *e) {
	describeTime("ctime", &e->st_ctime);
	describeTime("atime", &e->st_atime);
	describeTime("mtime", &e->st_mtime);
	checkpoint("    atime is a date only: %d, mtime second even: %d", timeOfDayZero(&e->st_atime), (e->st_mtime.second & 1) == 0);
}

static void compareEntries(const SceUtilitySavedataIdListEntry2 *a, const SceUtilitySavedataIdListEntry2 *b) {
	checkpoint("    same entry: %d (ctime same: %d, atime same: %d, mtime same: %d)",
		memcmp(a, b, sizeof(*a)) == 0, sameTime(&a->st_ctime, &b->st_ctime), sameTime(&a->st_atime, &b->st_atime), sameTime(&a->st_mtime, &b->st_mtime));
}

static void statSfo(SceIoStat *st) {
	memset(st, 0, sizeof(*st));
	int result = sceIoGetstat("ms0:/PSP/SAVEDATA/TEST99901ABC/PARAM.SFO", st);
	checkpoint("  sceIoGetstat PARAM.SFO: %08x", result);
}

static void cleanup() {
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901ABC/DATA.BIN");
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901ABC/PARAM.SFO");
	sceIoRmdir("ms0:/PSP/SAVEDATA/TEST99901ABC");
}

int main(int argc, char **argv) {
	SceUtilitySavedataParam2 param;
	SceUtilitySavedataIdListEntry2 made, again, afterRead, afterIoRead, afterWrite;
	SceIoStat st1, st2;

	initDisplay();
	cleanup();

	checkpointNext("Make the save");
	initParams(&param, SCE_UTILITY_SAVEDATA_TYPE_MAKEDATA);
	checkpoint("  MAKEDATA: %08x", runQuiet(&param));
	listEntry(&made);
	describeEntry(&made);

	// Past the FAT's 2 second mtime granularity, so a rewrite can't land on the same time.
	sceKernelDelayThread(3000000);

	checkpointNext("LIST again");
	listEntry(&again);
	compareEntries(&made, &again);

	checkpointNext("READDATA, then LIST");
	initParams(&param, SCE_UTILITY_SAVEDATA_TYPE_READDATA);
	param.dataBuf = readbuf;
	param.dataBufSize = sizeof(readbuf);
	param.dataSize = sizeof(readbuf);
	checkpoint("  READDATA: %08x", runQuiet(&param));
	listEntry(&afterRead);
	compareEntries(&made, &afterRead);

	checkpointNext("sceIoRead PARAM.SFO, then LIST");
	statSfo(&st1);
	SceUID fd = sceIoOpen("ms0:/PSP/SAVEDATA/TEST99901ABC/PARAM.SFO", PSP_O_RDONLY, 0);
	checkpoint("  sceIoRead: %d", sceIoRead(fd, readbuf, sizeof(readbuf)));
	sceIoClose(fd);
	statSfo(&st2);
	checkpoint("    stat atime same: %d, ctime same: %d, mtime same: %d",
		sameTime(&st1.sce_st_atime, &st2.sce_st_atime), sameTime(&st1.sce_st_ctime, &st2.sce_st_ctime), sameTime(&st1.sce_st_mtime, &st2.sce_st_mtime));
	checkpoint("    LIST mtime==SFO mtime: %d, LIST atime==SFO atime: %d",
		sameTime(&made.st_mtime, &st2.sce_st_mtime), sameTime(&made.st_atime, &st2.sce_st_atime));
	checkpoint("    SFO atime is a date only: %d", timeOfDayZero(&st2.sce_st_atime));
	listEntry(&afterIoRead);
	compareEntries(&made, &afterIoRead);

	checkpointNext("WRITEDATA, then LIST");
	initParams(&param, SCE_UTILITY_SAVEDATA_TYPE_WRITEDATA);
	checkpoint("  WRITEDATA: %08x", runQuiet(&param));
	listEntry(&afterWrite);
	// mtime moves on a PSP, but an emulator's file times follow the host clock, which may not have
	// left the two second step while emulated time ran ahead.
	checkpoint("    ctime same: %d, atime same: %d",
		sameTime(&made.st_ctime, &afterWrite.st_ctime), sameTime(&made.st_atime, &afterWrite.st_atime));
	describeEntry(&afterWrite);

	cleanup();
	return 0;
}
