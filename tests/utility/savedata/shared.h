#include <common.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#define sceUtilitySavedataUpdate sceUtilitySavedataUpdate_INCORRECT

#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspmoduleinfo.h>
#include <psputility.h>
#include <pspiofilemgr.h>
#include <pspthreadman.h>

#include <sysmem-imports.h>

#include <pspgu.h>
#include <pspdisplay.h>

#undef sceUtilitySavedataUpdate
extern int sceUtilitySavedataUpdate(int animSpeed);

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272

typedef char SceUtilitySavedataSaveName[20];

typedef struct SceUtilitySavedataMsFreeInfo2 {
	int clusterSize;
	int freeClusters;
	int freeSpaceKB;
	char freeSpaceStr[8];
	// TODO
	int unknownSafetyPad;
} SceUtilitySavedataMsFreeInfo2;

typedef struct SceUtilitySavedataUsedDataInfo2 {
	int usedClusters;
	int usedSpaceKB;
	char usedSpaceStr[8];
	int usedSpace32KB;
	char usedSpace32Str[8];
} SceUtilitySavedataUsedDataInfo2;

typedef struct SceUtilitySavedataMsDataInfo2 {
	char gameName[16];
	SceUtilitySavedataSaveName saveName;
	SceUtilitySavedataUsedDataInfo2 info;
} SceUtilitySavedataMsDataInfo2;

typedef struct SceUtilitySavedataIdListEntry2 {
	int st_mode;
	ScePspDateTime st_ctime;
	ScePspDateTime st_atime;
	ScePspDateTime st_mtime;
	SceUtilitySavedataSaveName name;
} SceUtilitySavedataIdListEntry2;

typedef struct SceUtilitySavedataIdListInfo2 {
	int maxCount;
	int resultCount;
	SceUtilitySavedataIdListEntry2 *entries;
} SceUtilitySavedataIdListInfo2;

typedef struct SceUtilitySavedataFileListEntry2 {
	int st_mode;
	int st_attr;
	u64 st_size;
	ScePspDateTime st_ctime;
	ScePspDateTime st_atime;
	ScePspDateTime st_mtime;
	char name[16];
} SceUtilitySavedataFileListEntry2;

typedef struct SceUtilitySavedataFileListInfo2 {
	int maxSecureEntries;
	int maxNormalEntries;
	int maxSystemEntries;
	int resultNumSecureEntries;
	int resultNumNormalEntries;
	int resultNumSystemEntries;
	SceUtilitySavedataFileListEntry2 *secureEntries;
	SceUtilitySavedataFileListEntry2 *normalEntries;
	SceUtilitySavedataFileListEntry2 *systemEntries;
} SceUtilitySavedataFileListInfo2;

typedef struct SceUtilitySavedataSizeEntry {
	u64 size;
	char name[16];
} SceUtilitySavedataSizeEntry;

typedef struct SceUtilitySavedataSizeInfo {
	int numSecureEntries;
	int numNormalEntries;
	SceUtilitySavedataSizeEntry *secureEntries;
	SceUtilitySavedataSizeEntry *normalEntries;
	int sectorSize;
	int freeSectors;
	int freeKB;
	char freeString[8];
	int neededKB;
	char neededString[8];
	int overwriteKB;
	char overwriteString[8];
} SceUtilitySavedataSizeInfo;

typedef struct SceUtilitySavedataParam2 {
	pspUtilityDialogCommon base;
	PspUtilitySavedataMode mode;
	int bind;
	int overwrite;
	char gameName[13];
	char reserved[3];
	SceUtilitySavedataSaveName saveName;
	SceUtilitySavedataSaveName *saveNameList;
	char fileName[13];
	char reserved1[3];
	void *dataBuf;
	SceSize dataBufSize;
	SceSize dataSize;
	PspUtilitySavedataSFOParam sfoParam;
	PspUtilitySavedataFileData icon0FileData;
	PspUtilitySavedataFileData icon1FileData;
	PspUtilitySavedataFileData pic1FileData;
	PspUtilitySavedataFileData snd0FileData;
	PspUtilitySavedataListSaveNewData *newData;
	PspUtilitySavedataFocus focus;
	int abortStatus;
	SceUtilitySavedataMsFreeInfo2 *msFree;
	SceUtilitySavedataMsDataInfo2 *msData;
	SceUtilitySavedataUsedDataInfo2 *utilityData;
	char key[16];

	int secureVersion;
	int multiStatus;
	SceUtilitySavedataIdListInfo2 *idList;
	SceUtilitySavedataFileListInfo2 *fileList;
	SceUtilitySavedataSizeInfo *sizeInfo;
} SceUtilitySavedataParam2;

typedef enum PspUtilitySavedataMode2 {
	SCE_UTILITY_SAVEDATA_TYPE_AUTOLOAD        = 0,
	SCE_UTILITY_SAVEDATA_TYPE_AUTOSAVE        = 1,
	SCE_UTILITY_SAVEDATA_TYPE_LOAD            = 2,
	SCE_UTILITY_SAVEDATA_TYPE_SAVE            = 3,
	SCE_UTILITY_SAVEDATA_TYPE_LISTLOAD        = 4,
	SCE_UTILITY_SAVEDATA_TYPE_LISTSAVE        = 5,
	SCE_UTILITY_SAVEDATA_TYPE_LISTDELETE      = 6,
	SCE_UTILITY_SAVEDATA_TYPE_LISTALLDELETE   = 7,
	SCE_UTILITY_SAVEDATA_TYPE_SIZES           = 8,
	SCE_UTILITY_SAVEDATA_TYPE_AUTODELETE      = 9,
	SCE_UTILITY_SAVEDATA_TYPE_DELETE          = 10,
	SCE_UTILITY_SAVEDATA_TYPE_LIST            = 11,
	SCE_UTILITY_SAVEDATA_TYPE_FILES           = 12,
	SCE_UTILITY_SAVEDATA_TYPE_MAKEDATASECURE  = 13,
	SCE_UTILITY_SAVEDATA_TYPE_MAKEDATA        = 14,
	SCE_UTILITY_SAVEDATA_TYPE_READDATASECURE  = 15,
	SCE_UTILITY_SAVEDATA_TYPE_READDATA        = 16,
	SCE_UTILITY_SAVEDATA_TYPE_WRITEDATASECURE = 17,
	SCE_UTILITY_SAVEDATA_TYPE_WRITEDATA       = 18,
	SCE_UTILITY_SAVEDATA_TYPE_ERASESECURE     = 19,
	SCE_UTILITY_SAVEDATA_TYPE_ERASE           = 20,
	SCE_UTILITY_SAVEDATA_TYPE_DELETEDATA      = 21,
	SCE_UTILITY_SAVEDATA_TYPE_GETSIZE         = 22,
} PspUtilitySavedataMode2;

void setLastSaveParam(SceUtilitySavedataParam2 *param);
void printSaveParamChanges(SceUtilitySavedataParam2 *param);
int checkpointStatusChange();
void checkpointResetForSavedata();
void checkpointExists(const SceUtilitySavedataParam2 *param);

// By default: TEST99901ABC/DATA.BIN.
void initStandardSavedataParams(SceUtilitySavedataParam2 *param);
void runStandardSavedataLoop(SceUtilitySavedataParam2 *param);

void initDisplay();
