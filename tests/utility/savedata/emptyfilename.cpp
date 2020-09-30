#include "emptyfilename.h"
#include <pspiofilemgr.h>
#include <pspthreadman.h>
#include <psputils.h>

static SceUtilitySavedataSaveName saveNameList[] = {
	"ABC",
	"F1",
	"M2",
	"L3",
	"\0",
};

static char filenames[][10] = { "TEST1.OBJ", "TEST2.OBJ" };
static char savedatas[][23] = { "Overwrote by savedata1", "Overwrote by savedata2" };
static char loaddata[23] = "Orignal data";

static void runQuietSavedataLoop(SceUtilitySavedataParam2 *param) {
	setLastSaveParam(param);

	sceKernelDelayThread(2000);
	int result = sceUtilitySavedataInitStart((SceUtilitySavedataParam *)param);
	checkpoint("sceUtilitySavedataInitStart(mode:%d): %08x", param->mode, result);
	if (result != 0) {
		sceUtilitySavedataGetStatus();
		return;
	}

	sceKernelDelayThread(2000);
	for (int i = 0; i < 400000; ++i) {
		int status = sceUtilitySavedataGetStatus();
		if (status == 3) {
			if (result != param->base.result) {
				checkpoint("result: %08x => %08x", result, param->base.result);
				result = param->base.result;
			}
			break;
		}

		if (status == 2) {
			sceUtilitySavedataUpdate(1);
		}

		if (result != param->base.result) {
			checkpoint("result: %08x => %08x", result, param->base.result);
			result = param->base.result;
		}

		sceKernelDelayThread(2000);
		checkpointResetForSavedata();
	}

	if (result != param->base.result) {
		checkpoint("result: %08x => %08x", result, param->base.result);
		result = param->base.result;
	}

	sceUtilitySavedataShutdownStart();

	if (result != param->base.result) {
		checkpoint("result: %08x => %08x", result, param->base.result);
		result = param->base.result;
	}

	while (sceUtilitySavedataGetStatus() != 0) {
		sceKernelDelayThread(2000);
	}
	if (result != param->base.result) {
		checkpoint("result: %08x => %08x", result, param->base.result);
		result = param->base.result;
	}
	sceKernelDelayThread(2000);
	if (result != param->base.result) {
		checkpoint("result: %08x => %08x", result, param->base.result);
		result = param->base.result;
	}
}

static bool createSave(int secureVersion, int savedataIdx) {
	SceUtilitySavedataParam2 param;
	initStandardSavedataParams(&param);
	strncpy(param.fileName, filenames[savedataIdx], sizeof(param.fileName) - 1);

	param.mode = (PspUtilitySavedataMode)SCE_UTILITY_SAVEDATA_TYPE_MAKEDATASECURE;
	param.saveNameList = saveNameList;
	param.secureVersion = secureVersion;

	param.dataBuf = savedatas[savedataIdx];
	param.dataBufSize = sizeof(savedatas[savedataIdx]);
	param.dataSize = sizeof(savedatas[savedataIdx]);

	runQuietSavedataLoop(&param);
	return param.base.result == 0;
}

static void showFiles() {
	SceUtilitySavedataFileListInfo fileList;
	SceUtilitySavedataFileListEntry secureEntries[5];
	SceUtilitySavedataFileListEntry normalEntries[5];
	SceUtilitySavedataFileListEntry systemEntries[5];

	SceUtilitySavedataParam2 param;
	initStandardSavedataParams(&param);

	param.mode = (PspUtilitySavedataMode)SCE_UTILITY_SAVEDATA_TYPE_FILES;
	param.saveNameList = NULL;
	param.secureVersion = 0;

	param.dataBuf = NULL;
	param.dataBufSize = 0;
	param.dataSize = 0;

	param.fileList = &fileList;
	param.fileList->maxSecureEntries = 5;
	param.fileList->maxNormalEntries = 5;
	param.fileList->maxSystemEntries = 5;

	memset(secureEntries, 0, sizeof(secureEntries));
	memset(normalEntries, 0, sizeof(normalEntries));
	memset(systemEntries, 0, sizeof(systemEntries));
	param.fileList->secureEntries = secureEntries;
	param.fileList->normalEntries = normalEntries;
	param.fileList->systemEntries = systemEntries;
	runQuietSavedataLoop(&param);

	checkpoint("Files:");
	for (int i = 0; i < param.fileList->resultNumSecureEntries; i++) {
		checkpoint("Secure:%s", param.fileList->secureEntries[i].name);
	}
	for (int i = 0; i < param.fileList->resultNumNormalEntries; i++) {
		checkpoint("Normal:%s", param.fileList->normalEntries[i].name);
	}
	for (int i = 0; i < param.fileList->resultNumSystemEntries; i++) {
		checkpoint("System:%s", param.fileList->systemEntries[i].name);
	}
}

static bool runTest(PspUtilitySavedataMode2 mode) {
	SceUtilitySavedataParam2 param;
	initStandardSavedataParams(&param);

	param.mode = (PspUtilitySavedataMode)mode;
	param.saveNameList = saveNameList;
	param.secureVersion = 0;
	strncpy(param.fileName, "", sizeof(param.fileName) - 1);

	param.dataBuf = loaddata;
	param.dataBufSize = sizeof(loaddata);
	param.dataSize = sizeof(loaddata);

	runQuietSavedataLoop(&param);
	showFiles();
	checkpoint("loaddata:%s", loaddata);

	return param.base.result == 0;
}

static void cleanupSaves() {
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901ABC/TEST1.OBJ");
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901ABC/TEST2.OBJ");
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901ABC/PARAM.SFO");
	sceIoRmdir("ms0:/PSP/SAVEDATA/TEST99901ABC");
}

void runEmptyFileNameTrials(const char *title, PspUtilitySavedataMode2 mode, int numCreateSave) {
	checkpointNext(title);
	for (int i = 0; i < numCreateSave; i++) {
		createSave(0, i);
	}
	runTest(mode);
	cleanupSaves();
}