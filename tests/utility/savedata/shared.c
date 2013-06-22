#include "shared.h"

unsigned int __attribute__((aligned(16))) list[262144];

static SceUtilitySavedataParam2 lastParam;
SceUtilitySavedataMsFreeInfo lastMsFree;
SceUtilitySavedataUsedDataInfo lastMsData;
SceUtilitySavedataUsedDataInfo lastUtilityData;

void setLastSaveParam(SceUtilitySavedataParam2 *param) {
	memcpy(&lastParam, param, sizeof(lastParam));
	if (param->msFree != NULL)
		memcpy(&lastMsFree, param->msFree, sizeof(lastMsFree));
	if (param->msData != NULL)
		memcpy(&lastMsData, param->msData, sizeof(lastMsData));
	if (param->utilityData != NULL)
		memcpy(&lastUtilityData, param->utilityData, sizeof(lastUtilityData));

	// TODO: Need to copy other ptr data, fileList, etc.
}

void printSaveParamChanges(SceUtilitySavedataParam2 *param) {
#define CHECK_CHANGE_U32(var) \
	if (param->var != lastParam.var) { \
		schedf("CHANGE: %s: %08x => %08x\n", #var, (unsigned int) lastParam.var, (unsigned int) param->var); \
		lastParam.var = param->var; \
	}

#define CHECK_CHANGE_U64(var) \
	if (param->var != lastParam.var) { \
		schedf("CHANGE: %s: %016llx => %016llx\n", #var, (unsigned long long) lastParam.var, (unsigned long long) param->var); \
		lastParam.var = param->var; \
	}

#define CHECK_CHANGE_STR(var) \
	if (strcmp((char *) param->var, (char *) lastParam.var)) { \
		schedf("CHANGE: %s: %s => %s\n", #var, (char *) lastParam.var, (char *) param->var); \
		strcpy((char *) &lastParam.var, (char *) &param->var); \
	}

#define CHECK_CHANGE_STRN(var, n) \
	if (strcmp((char *) param->var, (char *) lastParam.var)) { \
		schedf("CHANGE: %s: %.*s => %.*s\n", #var, n, (char *) lastParam.var, n, (char *) param->var); \
		strncpy((char *) &lastParam.var, (char *) &param->var, n); \
	}

#define CHECK_CHANGE_FILEDATA(var) \
	CHECK_CHANGE_U32(var.buf); \
	CHECK_CHANGE_U32(var.bufSize); \
	CHECK_CHANGE_U32(var.size); \
	CHECK_CHANGE_U32(var.unknown);


	CHECK_CHANGE_U32(base.size);
	CHECK_CHANGE_U32(base.language);
	CHECK_CHANGE_U32(base.buttonSwap);
	CHECK_CHANGE_U32(base.graphicsThread);
	CHECK_CHANGE_U32(base.accessThread);
	CHECK_CHANGE_U32(base.fontThread);
	CHECK_CHANGE_U32(base.soundThread);
	CHECK_CHANGE_U32(base.result);
	CHECK_CHANGE_U32(base.reserved[0]);
	CHECK_CHANGE_U32(base.reserved[1]);
	CHECK_CHANGE_U32(base.reserved[2]);
	CHECK_CHANGE_U32(base.reserved[3]);

	CHECK_CHANGE_U32(mode);
	CHECK_CHANGE_U32(bind);
	CHECK_CHANGE_U32(overwrite);
	CHECK_CHANGE_STR(gameName);
	CHECK_CHANGE_STR(reserved);
	CHECK_CHANGE_STR(saveName);
	CHECK_CHANGE_U32(saveNameList);
	// TODO
	if (param->saveNameList != NULL) {
		CHECK_CHANGE_STR(saveNameList);
	}
	CHECK_CHANGE_STR(fileName);
	CHECK_CHANGE_STR(reserved1);

	CHECK_CHANGE_U32(dataBuf);
	CHECK_CHANGE_U32(dataBufSize);
	CHECK_CHANGE_U32(dataSize);

	CHECK_CHANGE_STR(sfoParam.title);
	CHECK_CHANGE_STR(sfoParam.savedataTitle);
	CHECK_CHANGE_STR(sfoParam.detail);
	CHECK_CHANGE_U32(sfoParam.parentalLevel);
	CHECK_CHANGE_U32(sfoParam.unknown[0]);
	CHECK_CHANGE_U32(sfoParam.unknown[1]);
	CHECK_CHANGE_U32(sfoParam.unknown[2]);

	CHECK_CHANGE_FILEDATA(icon0FileData);
	CHECK_CHANGE_FILEDATA(icon1FileData);
	CHECK_CHANGE_FILEDATA(pic1FileData);
	CHECK_CHANGE_FILEDATA(snd0FileData);

	// TODO
	CHECK_CHANGE_U32(newData);

	CHECK_CHANGE_U32(focus);
	CHECK_CHANGE_U32(abortStatus);
	if (param->msFree != NULL) {
		CHECK_CHANGE_U32(msFree->clusterSize);
		CHECK_CHANGE_U32(msFree->freeClusters);
		CHECK_CHANGE_U32(msFree->freeSpaceKB);
		CHECK_CHANGE_STRN(msFree->freeSpaceStr, 8);
		CHECK_CHANGE_U32(msFree->unknownSafetyPad);
	}
	if (param->msData != NULL) {
		CHECK_CHANGE_U32(msData->usedClusters);
		CHECK_CHANGE_U32(msData->usedSpaceKB);
		CHECK_CHANGE_STRN(msData->usedSpaceStr, 8);
		CHECK_CHANGE_U32(msData->usedSpace32KB);
		CHECK_CHANGE_STRN(msData->usedSpace32Str, 8);
		CHECK_CHANGE_U64(msData->unknownSafetyPad[0]);
		CHECK_CHANGE_U64(msData->unknownSafetyPad[1]);
		CHECK_CHANGE_U64(msData->unknownSafetyPad[2]);
		CHECK_CHANGE_U64(msData->unknownSafetyPad[3]);
		CHECK_CHANGE_U64(msData->unknownSafetyPad2);
	}
	if (param->utilityData != NULL) {
		CHECK_CHANGE_U32(utilityData->usedClusters);
		CHECK_CHANGE_U32(utilityData->usedSpaceKB);
		CHECK_CHANGE_STRN(utilityData->usedSpaceStr, 8);
		CHECK_CHANGE_U32(utilityData->usedSpace32KB);
		CHECK_CHANGE_STRN(utilityData->usedSpace32Str, 8);
		CHECK_CHANGE_U64(utilityData->unknownSafetyPad[0]);
		CHECK_CHANGE_U64(utilityData->unknownSafetyPad[1]);
		CHECK_CHANGE_U64(utilityData->unknownSafetyPad[2]);
		CHECK_CHANGE_U64(utilityData->unknownSafetyPad[3]);
		CHECK_CHANGE_U64(utilityData->unknownSafetyPad2);
	}

	// TODO: key

	CHECK_CHANGE_U32(secureVersion);
	CHECK_CHANGE_U32(multiStatus);

	if (param->idList != NULL) {
		CHECK_CHANGE_U32(idList->maxCount);
		CHECK_CHANGE_U32(idList->resultCount);

		if (param->idList->entries != NULL) {
			CHECK_CHANGE_U32(idList->entries->st_mode);
			// TODO: st_ctime, etc.?
			CHECK_CHANGE_STR(idList->entries->name);
		}
	}
	// TODO
	CHECK_CHANGE_U32(fileList);
	// TODO
	CHECK_CHANGE_U32(sizeInfo);
}

int checkpointStatusChange() {
	static int lastStatus = -1;
	int status = sceUtilitySavedataGetStatus();
	if (status != lastStatus || status < 0) {
		checkpoint("sceUtilitySavedataGetStatus: %08x (duplicates skipped)", status);
		lastStatus = status;
	}
	return status;
}

void initDisplay()
{
	sceGuInit();
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888, NULL, BUF_WIDTH);
	sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, NULL, BUF_WIDTH);
	sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFinish();
	sceGuSync(0, 0);
 
	sceDisplaySetMode(0, SCR_WIDTH, SCR_HEIGHT);
	sceDisplayWaitVblankStart();
	sceGuDisplay(1);
}

void initStandardSavedataParams(SceUtilitySavedataParam2 *param) {
	memset(param, 0, sizeof(SceUtilitySavedataParam2));

	param->base.size = sizeof(SceUtilitySavedataParam2);
	param->base.language = 1;
	param->base.buttonSwap = 1;
	param->base.graphicsThread = 0x21;
	param->base.accessThread = 0x22;
	param->base.fontThread = 0x23;
	param->base.soundThread = 0x20;

#define SET_STR(str, val) \
	strncpy(param->str, val, sizeof(param->str) - 1);

	SET_STR(gameName, "TEST99901");
	SET_STR(saveName, "ABC");
	SET_STR(fileName, "DATA.BIN");

	SET_STR(sfoParam.title, "pspautotests");
	SET_STR(sfoParam.savedataTitle, "pspautotests test savedata");
	SET_STR(sfoParam.detail, "Created during an automated or manual test.");

	u32 temp_key[] = {0x12345678, 0x12345678, 0x12345678, 0x12345678};
	memcpy(&param->key[0], &temp_key[0], sizeof(param->key));
}

void checkpointExistsSaveName(SceUtilitySavedataParam2 *param, const char *saveName) {
	char temp[512];
	snprintf(temp, sizeof(temp), "ms0:/PSP/SAVEDATA/%s%s/%s", param->gameName, saveName, param->fileName);

	SceUID fd = sceIoOpen(temp, PSP_O_RDONLY, 0);
	if (fd >= 0) {
		checkpoint("  File exists: %s (%08x)", temp, sceIoClose(fd));
	} else {
		checkpoint("  Does not exist: %s (%08x)", temp, fd);
	}
}

void checkpointExists(SceUtilitySavedataParam2 *param) {
	checkpointNext("Checking for files:");
	checkpointExistsSaveName(param, param->saveName);

	if (param->saveNameList != NULL) {
		int i;
		for (i = 0; i < 100; ++i) {
			const char *saveName = param->saveNameList[i];
			if (saveName[0] == '\0') {
				break;
			}
			checkpointExistsSaveName(param, saveName);
		}
	}
}

// A little bit of a hack to have nice expected output.
extern volatile int didResched;
extern SceUID reschedThread;
void checkpointResetForSavedata() {
	didResched = 0;
	sceKernelStartThread(reschedThread, 0, NULL);
}

void runStandardSavedataLoop(SceUtilitySavedataParam2 *param) {
	setLastSaveParam(param);

	checkpointNext("Init");

	checkpoint("sceUtilitySavedataInitStart: %08x", sceUtilitySavedataInitStart((SceUtilitySavedataParam *) param));
	printSaveParamChanges(param);

	int i;
	int first = 1;
	for (i = 0; i < 400000; ++i) {
		int status = checkpointStatusChange();
		if (status == 3)
			break;
		printSaveParamChanges(param);

		if (status == 2) {
			int result = sceUtilitySavedataUpdate(1);
			if (result != 0 || first) {
				checkpoint("sceUtilitySavedataUpdate: %08x (duplicates skipped)", result);
				first = 0;
			}
		}
		printSaveParamChanges(param);

		sceKernelDelayThread(2000);
		checkpointResetForSavedata();
	}

	printSaveParamChanges(param);

	checkpoint("sceUtilitySavedataShutdownStart: %08x", sceUtilitySavedataShutdownStart());
	printSaveParamChanges(param);

	checkpointStatusChange();
	printSaveParamChanges(param);

	checkpoint("Delayed to allow for shutdown: %08x", sceKernelDelayThread(100000));

	checkpointStatusChange();
	printSaveParamChanges(param);
}