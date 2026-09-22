#pragma once

#define sceUtilityMsgDialogShutdownStart sceUtilityMsgDialogShutdownStart_WRONG
#define sceUtilityMsgDialogUpdate sceUtilityMsgDialogUpdate_WRONG
#define sceUtilitySavedataUpdate sceUtilitySavedataUpdate_WRONG
#define sceUtilityGameSharingShutdownStart sceUtilityGameSharingShutdownStart_WRONG
#define sceUtilityGameSharingUpdate sceUtilityGameSharingUpdate_WRONG

#include <common.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <psputility.h>

#undef sceUtilityMsgDialogShutdownStart
#undef sceUtilityMsgDialogUpdate
#undef sceUtilitySavedataUpdate
#undef sceUtilityGameSharingShutdownStart
#undef sceUtilityGameSharingUpdate

#ifdef __cplusplus
extern "C" {
#endif

int sceUtilityMsgDialogShutdownStart();
int sceUtilityMsgDialogUpdate(int animSpeed);
int sceUtilitySavedataUpdate(int animSpeed);
int sceUtilityGameSharingShutdownStart();
int sceUtilityGameSharingUpdate(int animSpeed);

// Not in the SDK headers, see utility-imports.S.
int sceUtilityScreenshotGetStatus();
int sceUtilityGamedataInstallGetStatus();
int sceUtilityNpSigninGetStatus();

#define MSGDIALOG_SIZE_V3 708

typedef struct {
	pspUtilityDialogCommon base;
	int result;
	int mode;
	int errorNum;
	char message[512];
	unsigned int options;
	unsigned int buttonPressed;
	char okayButton[64];
	char cancelButton[64];
} MsgDialogParams;

typedef struct {
	pspUtilityDialogCommon base;
	int mode;
	int bind;
	int overwrite;
	char gameName[13];
	char reserved[3];
	char saveName[20];
	void *saveNameList;
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
	void *newData;
	int focus;
	int abortStatus;
	void *msFree;
	void *msData;
	void *utilityData;
	char key[16];
	int secureVersion;
	int multiStatus;
	void *idList;
	void *fileList;
	void *sizeInfo;
} SavedataParams;

void initDisplay();
void initMsgParams(MsgDialogParams *p, int graphicsThread, int accessThread);
// GETSIZE on a game name that doesn't exist: no UI, and nothing is written.
void initSavedataParams(SavedataParams *p);

// All the GetStatus calls on one line.
void printAllStatus(const char *label);
const char *statusName(int status);

// Runs a started msg dialog through Abort, and a started savedata to FINISHED, then shuts down and
// waits for NONE. Return how many frames that took, or -1 if it didn't get there.
int finishMsg();
int finishSavedata();

#ifdef __cplusplus
}
#endif
