extern "C" {
#include "shared.h"
#include "sysmem-imports.h"
}
#include <pspiofilemgr.h>
#include <pspthreadman.h>
#include <psputils.h>

static SceUtilitySavedataSaveName saveNameList[] = {
	"F1",
	"M2",
	"M2",
	"L3",
	"\0",
};

static const u32 testKey[] = { 0x12345678, 0xAABBCCDD, 0x98765432, 0x1337C0DE };

static char savedata[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6
};

static char loaddata[16] = { };

void runQuietSavedataLoop(SceUtilitySavedataParam2 *param) {
	setLastSaveParam(param);

	sceKernelDelayThread(2000);
	int result = sceUtilitySavedataInitStart((SceUtilitySavedataParam *)param);
	if (result != 0) {
		sceUtilitySavedataGetStatus();
		return;
	}

	sceKernelDelayThread(2000);
	for (int i = 0; i < 400000; ++i) {
		int status = sceUtilitySavedataGetStatus();
		if (status == 3)
			break;

		if (status == 2) {
			sceUtilitySavedataUpdate(1);
		}

		sceKernelDelayThread(2000);
		checkpointResetForSavedata();
	}

	sceUtilitySavedataShutdownStart();
	while (sceUtilitySavedataGetStatus() == 3) {
		sceKernelDelayThread(2000);
	}
	sceKernelDelayThread(2000);
	sceUtilitySavedataGetStatus();
}

void cleanupSaves() {
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901ABC/DATA.BIN");
	sceIoRemove("ms0:/PSP/SAVEDATA/TEST99901ABC/PARAM.SFO");
	sceIoRmdir("ms0:/PSP/SAVEDATA/TEST99901ABC");
}

bool createSave(const char *ver, int secureVersion, bool key = true) {
	SceUtilitySavedataParam2 param;
	initStandardSavedataParams(&param);

	param.mode = PSP_UTILITY_SAVEDATA_AUTOSAVE;
	param.saveNameList = saveNameList;
	param.secureVersion = secureVersion;

	param.dataBuf = savedata;
	param.dataBufSize = sizeof(savedata);
	param.dataSize = sizeof(savedata);

	if (!key) {
		memset(param.key, 0, sizeof(param.key));
	} else {
		memcpy(param.key, testKey, sizeof(param.key));
	}

	runQuietSavedataLoop(&param);
	if (param.base.result != 0) {
		checkpoint("%s   Save error with secureVersion=%d: %08x", ver, secureVersion, param.base.result);
	}
	return param.base.result == 0;
}

bool loadSave(const char *ver, const char *title, int secureVersion, bool key = true) {
	SceUtilitySavedataParam2 param;
	initStandardSavedataParams(&param);

	param.mode = PSP_UTILITY_SAVEDATA_AUTOLOAD;
	param.saveNameList = saveNameList;
	param.secureVersion = secureVersion;

	memset(loaddata, 0xCC, sizeof(loaddata));
	sceKernelDcacheWritebackInvalidateAll();
	param.dataBuf = loaddata;
	param.dataBufSize = sizeof(loaddata);
	param.dataSize = sizeof(loaddata);

	if (!key) {
		memset(param.key, 0, sizeof(param.key));
	} else {
		memcpy(param.key, testKey, sizeof(param.key));
	}

	runQuietSavedataLoop(&param);
	if (param.base.result != 0) {
		checkpoint("%s %s: Load error: %08x", ver, title, param.base.result);
		return false;
	}

	sceKernelDcacheWritebackInvalidateAll();
	if (memcmp(savedata, loaddata, sizeof(loaddata)) != 0) {
		checkpoint("%s %s: Data did not match: %02x, %02x", ver, title, savedata[0], loaddata[0]);
		return false;
	}
	checkpoint("%s %s: Data matched", ver, title);
	return true;
}

struct SFOHeader {
	u32 magic;
	u32 version;
	u32 key_table_start;
	u32 data_table_start;
	u32 index_table_entries;
};

struct SFOIndexTable {
	u16 key_table_offset;
	u16 param_fmt;
	u32 param_len;
	u32 param_max_len;
	u32 data_table_offset;
};

int ReadSavedataMode(const u8 *paramsfo, size_t size) {
	if (size < sizeof(SFOHeader))
		return -1;
	const SFOHeader *header = (const SFOHeader *)paramsfo;
	if (header->magic != 0x46535000)
		return -1;

	const SFOIndexTable *indexTables = (const SFOIndexTable *)(paramsfo + sizeof(SFOHeader));

	const u8 *key_start = paramsfo + header->key_table_start;
	const u8 *data_start = paramsfo + header->data_table_start;

	for (u32 i = 0; i < header->index_table_entries; i++) {
		const char *key = (const char *)(key_start + indexTables[i].key_table_offset);
		if (strcmp(key, "SAVEDATA_PARAMS") != 0 || indexTables[i].param_fmt != 4) {
			continue;
		}

		const u8 *utfdata = (const u8 *)(data_start + indexTables[i].data_table_offset);
		if (indexTables[i].param_len >= 1) {
			return utfdata[0];
		}
	}

	return -1;
}

void checkSFO(const char *ver, const char *title) {
	SceIoStat sta;
	sceIoGetstat("ms0:/PSP/SAVEDATA/TEST99901ABC/PARAM.SFO", &sta);

	int fd = sceIoOpen("ms0:/PSP/SAVEDATA/TEST99901ABC/PARAM.SFO", PSP_O_RDONLY, 0777);
	u8 *data = new u8[sta.st_size];
	sceIoRead(fd, data, sta.st_size);
	sceIoClose(fd);

	int mode = ReadSavedataMode(data, sta.st_size);
	checkpoint("%s %s: SFO mode %02x", ver, title, mode);

	delete [] data;
}

void checkpointNext2(const char *a, const char *b) {
	char temp[1024];
	snprintf(temp, sizeof(temp), "%s %s", a, b);
	checkpointNext(temp);
}

void runTrials(const char *ver) {
	checkpointNext2(ver, "Save version 0 (auto)");
	if (createSave(ver, 0)) {
		checkSFO(ver, "  Version 0");
		loadSave(ver, "  Version 0/0", 0);
		loadSave(ver, "  Version 0/1", 1);
		loadSave(ver, "  Version 0/2", 2);
		loadSave(ver, "  Version 0/3", 3);
		loadSave(ver, "  Version 0/4", 4);
		loadSave(ver, "  Version 0/5", 5);
		loadSave(ver, "  Version 0/6", 6);
		loadSave(ver, "  Version 0/-1", -1);
	}
	cleanupSaves();

	checkpointNext2(ver, "Save version 0 (no key)");
	if (createSave(ver, 0, false)) {
		checkSFO(ver, "  Version 0 (no key)");
		loadSave(ver, "  Version 0/0 (no key)", 0, false);
		loadSave(ver, "  Version 0/1 (no key)", 1, false);
		loadSave(ver, "  Version 0/2 (no key)", 2, false);
		loadSave(ver, "  Version 0/3 (no key)", 3, false);
	}
	cleanupSaves();

	checkpointNext2(ver, "Save version 1");
	if (createSave(ver, 1)) {
		checkSFO(ver, "  Version 1");
		loadSave(ver, "  Version 1/0", 0);
		loadSave(ver, "  Version 1/1", 1);
		loadSave(ver, "  Version 1/2", 2);
		loadSave(ver, "  Version 1/3", 3);
	}
	cleanupSaves();

	checkpointNext2(ver, "Save version 1 (no key)");
	if (createSave(ver, 1, false)) {
		checkSFO(ver, "  Version 1 (no key)");
		loadSave(ver, "  Version 1/0 (no key)", 0, false);
		loadSave(ver, "  Version 1/1 (no key)", 1, false);
		loadSave(ver, "  Version 1/2 (no key)", 2, false);
		loadSave(ver, "  Version 1/3 (no key)", 3, false);
	}
	cleanupSaves();

	checkpointNext2(ver, "Save version 2");
	if (createSave(ver, 2)) {
		checkSFO(ver, "  Version 2");
		loadSave(ver, "  Version 2/0", 0);
		loadSave(ver, "  Version 2/1", 1);
		loadSave(ver, "  Version 2/2", 2);
		loadSave(ver, "  Version 2/3", 3);
	}
	cleanupSaves();

	checkpointNext2(ver, "Save version 2 (no key)");
	if (createSave(ver, 2, false)) {
		checkSFO(ver, "  Version 2 (no key)");
		loadSave(ver, "  Version 2/0 (no key)", 0, false);
		loadSave(ver, "  Version 2/1 (no key)", 1, false);
		loadSave(ver, "  Version 2/2 (no key)", 2, false);
		loadSave(ver, "  Version 2/3 (no key)", 3, false);
	}
	cleanupSaves();

	checkpointNext2(ver, "Save version 3");
	if (createSave(ver, 3)) {
		checkSFO(ver, "  Version 3");
		loadSave(ver, "  Version 3/0", 0);
		loadSave(ver, "  Version 3/1", 1);
		loadSave(ver, "  Version 3/2", 2);
		loadSave(ver, "  Version 3/3", 3);
	}
	cleanupSaves();

	checkpointNext2(ver, "Save version 3 (no key)");
	if (createSave(ver, 3, false)) {
		checkSFO(ver, "  Version 3 (no key)");
		loadSave(ver, "  Version 3/0 (no key)", 0, false);
		loadSave(ver, "  Version 3/1 (no key)", 1, false);
		loadSave(ver, "  Version 3/2 (no key)", 2, false);
		loadSave(ver, "  Version 3/3 (no key)", 3, false);
	}
	cleanupSaves();

	checkpointNext2(ver, "Save version 4");
	if (createSave(ver, 4)) {
		checkpoint("  Version 4: UNEXPECTED");
	}
	cleanupSaves();

	checkpointNext2(ver, "Save version 5");
	if (createSave(ver, 5)) {
		checkpoint("  Version 5: UNEXPECTED");
	}
	cleanupSaves();

	checkpointNext2(ver, "Save version 6");
	if (createSave(ver, 6)) {
		checkpoint("  Version 6: UNEXPECTED");
	}
	cleanupSaves();

	checkpointNext2(ver, "Save version -1");
	if (createSave(ver, -1)) {
		checkpoint("  Version -1: UNEXPECTED");
	}
	cleanupSaves();
}

extern "C" int main(int argc, char **argv) {
	initDisplay();

	sceKernelSetCompiledSdkVersion606(0x06060010);
	runTrials("6.60");

	sceKernelSetCompiledSdkVersion603_605(0x6050010);
	runTrials("6.50");

	sceKernelSetCompiledSdkVersion507(0x05070010);
	runTrials("5.70");

	sceKernelSetCompiledSdkVersion380_390(0x3080010);
	runTrials("3.80");

	sceKernelSetCompiledSdkVersion370(0x03070010);
	runTrials("3.70");

	sceKernelSetCompiledSdkVersion(0x03000010);
	runTrials("3.00");

	sceKernelSetCompiledSdkVersion(0x02060010);
	runTrials("2.60");

	sceKernelSetCompiledSdkVersion(0x02050010);
	runTrials("2.50");

	sceKernelSetCompiledSdkVersion(0x02000010);
	runTrials("2.00");

	sceKernelSetCompiledSdkVersion(0x01050010);
	runTrials("1.50");

	// save 3 maps to v5 for >= 3.xx only, v1 otherwise
	// save 2 maps to v3 (all versions)
	// save 1 maps to v1 (all versions)

	return 0;
}
