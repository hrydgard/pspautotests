extern "C" {
#include "shared.h"
}
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

static char savedata[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6
};

static char loaddata[16] = { };

void runQuietSavedataLoop(SceUtilitySavedataParam2 *param) {
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

bool createSave(int secureVersion) {
	SceUtilitySavedataParam2 param;
	initStandardSavedataParams(&param);

	param.mode = (PspUtilitySavedataMode)SCE_UTILITY_SAVEDATA_TYPE_MAKEDATASECURE;
	param.saveNameList = saveNameList;
	param.secureVersion = secureVersion;

	param.dataBuf = savedata;
	param.dataBufSize = sizeof(savedata);
	param.dataSize = sizeof(savedata);

	runQuietSavedataLoop(&param);
	return param.base.result == 0;
}

void showFiles() {
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

bool deleteSave(PspUtilitySavedataMode2 mode) {
	SceUtilitySavedataParam2 param;
	initStandardSavedataParams(&param);

	param.mode = (PspUtilitySavedataMode)mode;
	param.saveNameList = saveNameList;
	param.secureVersion = 0;

	param.dataBuf = loaddata;
	param.dataBufSize = sizeof(loaddata);
	param.dataSize = sizeof(loaddata);
	runQuietSavedataLoop(&param);
	showFiles();

	return param.base.result == 0;
}

void cleanupSaves() {
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901ABC/DATA.BIN");
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901ABC/PARAM.SFO");
	sceIoRmdir("ms0:/PSP/SAVEDATA/TEST99901ABC");
}

void runTrials(const char *title, PspUtilitySavedataMode2 mode) {
	checkpointNext(title);
	createSave(0);
	deleteSave(mode);
	cleanupSaves();
}

extern "C" int main(int argc, char **argv) {
	initDisplay();

	runTrials("auto delete:", SCE_UTILITY_SAVEDATA_TYPE_AUTODELETE);
	runTrials("delete:", SCE_UTILITY_SAVEDATA_TYPE_DELETE);
	runTrials("list delete:", SCE_UTILITY_SAVEDATA_TYPE_LISTDELETE);
	runTrials("list all delete:", SCE_UTILITY_SAVEDATA_TYPE_LISTALLDELETE);
	runTrials("delete data:", SCE_UTILITY_SAVEDATA_TYPE_DELETEDATA);
	runTrials("erase secure:", SCE_UTILITY_SAVEDATA_TYPE_ERASESECURE);
	runTrials("erase:", SCE_UTILITY_SAVEDATA_TYPE_ERASE);

	return 0;
}