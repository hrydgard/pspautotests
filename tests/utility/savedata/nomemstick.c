// What the savedata modes that don't show a dialog return, and what the memory stick devctls and
// ms0: report, depending on whether a memory stick is inserted. Run it once with the stick in and
// once with it pulled out (after PSPLink has started); the first line says which run it was.

#include "shared.h"

static SceUtilitySavedataSaveName saveNameList[] = {
	"ABC",
	"\0",
};

static char savedata[16] = { 1, 2, 3, 4 };

static const struct {
	int mode;
	const char *name;
} modes[] = {
	{ SCE_UTILITY_SAVEDATA_TYPE_AUTOLOAD, "AUTOLOAD" },
	{ SCE_UTILITY_SAVEDATA_TYPE_SIZES, "SIZES" },
	{ SCE_UTILITY_SAVEDATA_TYPE_LIST, "LIST" },
	{ SCE_UTILITY_SAVEDATA_TYPE_FILES, "FILES" },
	{ SCE_UTILITY_SAVEDATA_TYPE_READDATA, "READDATA" },
	{ SCE_UTILITY_SAVEDATA_TYPE_READDATASECURE, "READDATASECURE" },
	{ SCE_UTILITY_SAVEDATA_TYPE_GETSIZE, "GETSIZE" },
	{ SCE_UTILITY_SAVEDATA_TYPE_ERASE, "ERASE" },
	{ SCE_UTILITY_SAVEDATA_TYPE_ERASESECURE, "ERASESECURE" },
	{ SCE_UTILITY_SAVEDATA_TYPE_DELETEDATA, "DELETEDATA" },
	{ SCE_UTILITY_SAVEDATA_TYPE_AUTODELETE, "AUTODELETE" },
	{ SCE_UTILITY_SAVEDATA_TYPE_WRITEDATA, "WRITEDATA" },
	{ SCE_UTILITY_SAVEDATA_TYPE_WRITEDATASECURE, "WRITEDATASECURE" },
	{ SCE_UTILITY_SAVEDATA_TYPE_MAKEDATA, "MAKEDATA" },
	{ SCE_UTILITY_SAVEDATA_TYPE_MAKEDATASECURE, "MAKEDATASECURE" },
	{ SCE_UTILITY_SAVEDATA_TYPE_AUTOSAVE, "AUTOSAVE" },
};

static SceUtilitySavedataMsFreeInfo2 msFree;
static SceUtilitySavedataMsDataInfo2 msData;
static SceUtilitySavedataUsedDataInfo2 utilityData;
static SceUtilitySavedataIdListEntry2 idEntries[8];
static SceUtilitySavedataIdListInfo2 idList;
static SceUtilitySavedataFileListEntry2 fileEntries[8];
static SceUtilitySavedataFileListInfo2 fileList;
static SceUtilitySavedataSizeEntry sizeEntries[1];
static SceUtilitySavedataSizeInfo sizeInfo;

static void runMode(int mode, const char *name) {
	SceUtilitySavedataParam2 param;
	initStandardSavedataParams(&param);
	param.mode = (PspUtilitySavedataMode)mode;
	param.saveNameList = saveNameList;
	param.dataBuf = savedata;
	param.dataBufSize = sizeof(savedata);
	param.dataSize = sizeof(savedata);

	memset(&msFree, 0, sizeof(msFree));
	memset(&msData, 0, sizeof(msData));
	memset(&utilityData, 0, sizeof(utilityData));
	if (mode == SCE_UTILITY_SAVEDATA_TYPE_SIZES) {
		param.msFree = &msFree;
		param.msData = &msData;
		param.utilityData = &utilityData;
	}
	strcpy(msData.gameName, "TEST99901");
	strcpy(msData.saveName, "ABC");

	memset(&idList, 0, sizeof(idList));
	idList.maxCount = 8;
	idList.entries = idEntries;
	if (mode == SCE_UTILITY_SAVEDATA_TYPE_LIST) {
		param.idList = &idList;
	}
	strcpy(param.saveName, mode == SCE_UTILITY_SAVEDATA_TYPE_LIST ? "<>" : "ABC");

	memset(&fileList, 0, sizeof(fileList));
	fileList.maxSecureEntries = 8;
	fileList.maxNormalEntries = 8;
	fileList.maxSystemEntries = 8;
	fileList.secureEntries = fileEntries;
	fileList.normalEntries = fileEntries;
	fileList.systemEntries = fileEntries;
	if (mode == SCE_UTILITY_SAVEDATA_TYPE_FILES) {
		param.fileList = &fileList;
	}

	memset(&sizeInfo, 0, sizeof(sizeInfo));
	sizeInfo.numNormalEntries = 1;
	sizeInfo.normalEntries = sizeEntries;
	strcpy(sizeEntries[0].name, "DATA.BIN");
	sizeEntries[0].size = sizeof(savedata);
	if (mode == SCE_UTILITY_SAVEDATA_TYPE_GETSIZE) {
		param.sizeInfo = &sizeInfo;
	}

	int result = sceUtilitySavedataInitStart((SceUtilitySavedataParam *)&param);
	if (result != 0) {
		printf("  %s: init %08x\n", name, result);
		return;
	}

	int status = 0;
	int i;
	// Bounded, so a mode that never finishes is reported rather than hanging. SIZES scans the
	// whole memory stick, which takes a few seconds on a full one.
	for (i = 0; i < 7500; ++i) {
		status = sceUtilitySavedataGetStatus();
		if (status == 3)
			break;
		if (status == 2)
			sceUtilitySavedataUpdate(1);
		sceKernelDelayThread(2000);
	}
	if (status != 3) {
		printf("  %s: stuck in status %d\n", name, status);
		return;
	}
	sceUtilitySavedataShutdownStart();
	for (i = 0; i < 500 && sceUtilitySavedataGetStatus() != 0; ++i) {
		sceKernelDelayThread(2000);
	}
	printf("  %s: result %08x\n", name, param.base.result);
}

static void cleanup() {
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901ABC/DATA.BIN");
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901ABC/PARAM.SFO");
	sceIoRmdir("ms0:/PSP/SAVEDATA/TEST99901ABC");
}

int main(int argc, char **argv) {
	int state = -1;
	int fatState = -1;
	int stateResult = sceIoDevctl("mscmhc0:", 0x02025806, NULL, 0, &state, sizeof(state));
	int fatResult = sceIoDevctl("fatms0:", 0x02425823, NULL, 0, &fatState, sizeof(fatState));
	printf("Memory stick %s\n", state == 1 ? "inserted" : "not inserted");
	printf("  mscmhc0: 02025806: %08x, state %d\n", stateResult, state);
	printf("  fatms0: 02425823: %08x, state %d\n", fatResult, fatState);

	SceIoStat st;
	printf("  sceIoGetstat(ms0:/PSP): %08x\n", sceIoGetstat("ms0:/PSP", &st) < 0 ? sceIoGetstat("ms0:/PSP", &st) : 0);
	SceUID dfd = sceIoDopen("ms0:/PSP/SAVEDATA");
	printf("  sceIoDopen(ms0:/PSP/SAVEDATA): %08x\n", dfd < 0 ? dfd : 0);
	if (dfd >= 0) {
		sceIoDclose(dfd);
	}
	SceUID fd = sceIoOpen("ms0:/nomemstick.txt", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	printf("  sceIoOpen(ms0:/nomemstick.txt, create): %08x\n", fd < 0 ? fd : 0);
	if (fd >= 0) {
		sceIoClose(fd);
		sceIoRemove("ms0:/nomemstick.txt");
	}

	initDisplay();
	cleanup();

	// Without any data first, then the same again once WRITEDATA, MAKEDATA or AUTOSAVE made some.
	printf("Modes:\n");
	int i;
	for (i = 0; i < ARRAY_SIZE(modes); ++i) {
		runMode(modes[i].mode, modes[i].name);
	}
	printf("Modes, after saving:\n");
	for (i = 0; i < ARRAY_SIZE(modes); ++i) {
		runMode(modes[i].mode, modes[i].name);
	}

	cleanup();
	return 0;
}
