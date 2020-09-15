// Note:Use pspauototest, should give enough time to complete this test
// e.g. gentest.py -t 1000 utility\savedata\loaddata

extern "C" {
#include "shared.h"
}
#include <pspiofilemgr.h>
#include <pspthreadman.h>
#include <psputils.h>

enum LoadSaveMode {
	SAVEDATA_TYPE_AUTOLOAD = 0x01,
	SAVEDATA_TYPE_LOAD = 0x02,
	SAVEDATA_TYPE_LISTLOAD = 0x04,
	SAVEDATA_TYPE_READDATASECURE = 0x08,
	SAVEDATA_TYPE_READDATA = 0x10,
	
	SAVENAME_NONE = 0x0100,
	SAVENAME_NOT_IN_LIST = 0x0200,
	SAVENAME_IN_LIST_FIRST = 0x0400,
	SAVENAME_IN_LIST_NOT_FIRST = 0x0800,
	SAVENAME_ANYTHING = 0x1000,

	SAVENAMELIST_NONE = 0x010000,
};

#define LOADSAVEMODE_TYPE_MASK 0xFF
#define LOADSAVEMODE_SAVENAME_MASK 0xFF00
#define LOADSAVEMODE_SAVENAMELIST_MASK 0xFF0000

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
	checkpointExists(&param);
	return param.base.result == 0;
}

bool createSave(SceUtilitySavedataParam2 param) {
	param.mode = (PspUtilitySavedataMode)SCE_UTILITY_SAVEDATA_TYPE_MAKEDATASECURE;
	param.saveNameList = saveNameList;

	param.dataBuf = savedata;
	param.dataBufSize = sizeof(savedata);
	param.dataSize = sizeof(savedata);

	param.base.result = 0; // reset the result.


	runQuietSavedataLoop(&param);
	checkpointExists(&param);

	return param.base.result == 0;
}

bool loadSave(int secureVersion, u32 mode) {
	SceUtilitySavedataParam2 param;
	bool needCreateSave = false;
	initStandardSavedataParams(&param);

	param.secureVersion = secureVersion;
	u32 typeMode = mode & LOADSAVEMODE_TYPE_MASK;
	u32 savenameMode = mode & LOADSAVEMODE_SAVENAME_MASK;
	u32 savenamelistMode = mode & LOADSAVEMODE_SAVENAMELIST_MASK;

	switch (typeMode) {
	case SAVEDATA_TYPE_AUTOLOAD:
		param.mode = (PspUtilitySavedataMode)SCE_UTILITY_SAVEDATA_TYPE_AUTOLOAD;
		break;
	case SAVEDATA_TYPE_LOAD:
		param.mode = (PspUtilitySavedataMode)SCE_UTILITY_SAVEDATA_TYPE_LOAD;
		break;
	case SAVEDATA_TYPE_LISTLOAD:
		param.mode = (PspUtilitySavedataMode)SCE_UTILITY_SAVEDATA_TYPE_LISTLOAD;
		break;
	case SAVEDATA_TYPE_READDATASECURE:
		param.mode = (PspUtilitySavedataMode)SCE_UTILITY_SAVEDATA_TYPE_READDATASECURE;
		break;
	case SAVEDATA_TYPE_READDATA:
		param.mode = (PspUtilitySavedataMode)SCE_UTILITY_SAVEDATA_TYPE_READDATA;
		break;
	default:
		param.mode = (PspUtilitySavedataMode)SCE_UTILITY_SAVEDATA_TYPE_AUTOLOAD;
		break;
	}

	switch (savenameMode) {
	case SAVENAME_NONE:
		strncpy(param.saveName, "", sizeof(param.saveName) - 1);
		needCreateSave = true;
		break;
	case SAVENAME_NOT_IN_LIST:
		strncpy(param.saveName, "E0", sizeof(param.saveName) - 1);
		needCreateSave = true;
		break;
	case SAVENAME_IN_LIST_FIRST:
		break;// Done
	case SAVENAME_IN_LIST_NOT_FIRST:
		strncpy(param.saveName, "F1", sizeof(param.saveName) - 1);
		needCreateSave = true;
		break;
	case SAVENAME_ANYTHING:
		strncpy(param.saveName, "<>", sizeof(param.saveName) - 1);
		break;
	default:
		break;
	}

	switch (savenamelistMode) {
	case SAVENAMELIST_NONE:
		param.saveNameList = NULL;
	default:
		param.saveNameList = saveNameList;
		break;
	}
	
	param.dataBuf = loaddata;
	param.dataBufSize = sizeof(loaddata);
	param.dataSize = sizeof(loaddata);

	runQuietSavedataLoop(&param);
	if (needCreateSave) {
		createSave(param);
		param.base.result = 0; // reset the result.
		runQuietSavedataLoop(&param);
	}

	return param.base.result == 0;
}

void cleanupSaves() {
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901ABC/DATA.BIN");
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901ABC/PARAM.SFO");
	sceIoRmdir("ms0:/PSP/SAVEDATA/TEST99901ABC");
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901/DATA.BIN");
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901/PARAM.SFO");
	sceIoRmdir("ms0:/PSP/SAVEDATA/TEST99901");
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901F1/DATA.BIN");
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901F1/PARAM.SFO");
	sceIoRmdir("ms0:/PSP/SAVEDATA/TEST99901F1");
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901E0/DATA.BIN");
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901E0/PARAM.SFO");
	sceIoRmdir("ms0:/PSP/SAVEDATA/TEST99901E0");
}

void runTrials(const char *title, u32 mode) {
	checkpointNext(title);
	createSave(0);
	loadSave(0, mode);
	cleanupSaves();
}

void testSaveName() {
	runTrials("no save name(auto load):", SAVEDATA_TYPE_AUTOLOAD | SAVENAME_NONE);
	runTrials("save name not in the list(auto load):", SAVEDATA_TYPE_AUTOLOAD | SAVENAME_NOT_IN_LIST);
	runTrials("save name is the first in the list(auto load):", SAVEDATA_TYPE_AUTOLOAD | SAVENAME_IN_LIST_FIRST);
	runTrials("save name is in the list but not first(auto load):", SAVEDATA_TYPE_AUTOLOAD | SAVENAME_IN_LIST_NOT_FIRST);
	runTrials("save name could be anything(auto load):", SAVEDATA_TYPE_AUTOLOAD | SAVENAME_ANYTHING);

	runTrials("no save name(load):", SAVEDATA_TYPE_LOAD | SAVENAME_NONE);
	runTrials("save name not in the list(load):", SAVEDATA_TYPE_LOAD | SAVENAME_NOT_IN_LIST);
	runTrials("save name is the first in the list(load):", SAVEDATA_TYPE_LOAD | SAVENAME_IN_LIST_FIRST);
	runTrials("save name is in the list but not first(load):", SAVEDATA_TYPE_LOAD | SAVENAME_IN_LIST_NOT_FIRST);
	runTrials("save name could be anything(load):", SAVEDATA_TYPE_LOAD | SAVENAME_ANYTHING);

	runTrials("no save name(list load):", SAVEDATA_TYPE_LISTLOAD | SAVENAME_NONE);
	runTrials("save name not in the list(list load):", SAVEDATA_TYPE_LISTLOAD | SAVENAME_NOT_IN_LIST);
	runTrials("save name is the first in the list(list load):", SAVEDATA_TYPE_LISTLOAD | SAVENAME_IN_LIST_FIRST);
	runTrials("save name is in the list but not first(list load):", SAVEDATA_TYPE_LISTLOAD | SAVENAME_IN_LIST_NOT_FIRST);
	runTrials("save name could be anything(list load):", SAVEDATA_TYPE_LISTLOAD | SAVENAME_ANYTHING);

	runTrials("no save name(read data secure):", SAVEDATA_TYPE_READDATASECURE | SAVENAME_NONE);
	runTrials("save name not in the list(read data secure):", SAVEDATA_TYPE_READDATASECURE | SAVENAME_NOT_IN_LIST);
	runTrials("save name is the first in the list(read data secure):", SAVEDATA_TYPE_READDATASECURE | SAVENAME_IN_LIST_FIRST);
	runTrials("save name is in the list but not first(read data secure):", SAVEDATA_TYPE_READDATASECURE | SAVENAME_IN_LIST_NOT_FIRST);
	runTrials("save name could be anything(read data secure):", SAVEDATA_TYPE_READDATASECURE | SAVENAME_ANYTHING);

	runTrials("no save name(read data):", SAVEDATA_TYPE_READDATA | SAVENAME_NONE);
	runTrials("save name not in the list(read data):", SAVEDATA_TYPE_READDATA | SAVENAME_NOT_IN_LIST);
	runTrials("save name is the first in the list(read data):", SAVEDATA_TYPE_READDATA | SAVENAME_IN_LIST_FIRST);
	runTrials("save name is in the list but not first(read data):", SAVEDATA_TYPE_READDATA | SAVENAME_IN_LIST_NOT_FIRST);
	runTrials("save name could be anything(read data):", SAVEDATA_TYPE_READDATA | SAVENAME_ANYTHING);
}

void testSaveNameList() {
	runTrials("no save name list(auto load):", SAVEDATA_TYPE_AUTOLOAD | SAVENAMELIST_NONE);
	runTrials("no save name list(load):", SAVEDATA_TYPE_LOAD | SAVENAMELIST_NONE);
	runTrials("no save name list(list load):", SAVEDATA_TYPE_LISTLOAD | SAVENAMELIST_NONE);
	runTrials("no save name list(read data secure):", SAVEDATA_TYPE_READDATASECURE | SAVENAMELIST_NONE);
	runTrials("no save name list(read data):", SAVEDATA_TYPE_READDATA | SAVENAMELIST_NONE);
}

extern "C" int main(int argc, char **argv) {
	initDisplay();

	testSaveName();
	testSaveNameList();

	return 0;
}