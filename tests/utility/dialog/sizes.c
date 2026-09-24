#include "shared.h"
#include <string.h>

// Which request sizes each dialog's InitStart rejects. The sizes sceUtility_Driver accepts (from its
// InitStart) are skipped, so nothing ever starts: every call should fail the size check.

int sceUtilityScreenshotInitStart(void *params);
int sceUtilityGamedataInstallInitStart(void *params);
int sceUtilityNpSigninInitStart(void *params);

static unsigned int __attribute__((aligned(16))) params[0x1000 / 4];

typedef int (*InitStartFunc)(void *params);

static int netconfInit(void *p) { return sceUtilityNetconfInitStart((pspUtilityNetconfData *)p); }
static int oskInit(void *p) { return sceUtilityOskInitStart((SceUtilityOskParams *)p); }
static int msgInit(void *p) { return sceUtilityMsgDialogInitStart((pspUtilityMsgDialogParams *)p); }
static int savedataInit(void *p) { return sceUtilitySavedataInitStart((SceUtilitySavedataParam *)p); }
static int sharingInit(void *p) { return sceUtilityGameSharingInitStart((pspUtilityGameSharingParams *)p); }

static void sweep(const char *name, InitStartFunc func, const unsigned int *valid, int validCount) {
	printf("%s:\n", name);
	unsigned int size;
	unsigned int runStart = 0;
	int runResult = 0;
	for (size = 0; size <= 0x801; ++size) {
		int i, skip = 0;
		for (i = 0; i < validCount; ++i) {
			if (valid[i] == size) {
				skip = 1;
			}
		}

		int result = runResult;
		if (size <= 0x800 && !skip) {
			memset(params, 0, sizeof(params));
			params[0] = size;
			result = func(params);
			if (result >= 0) {
				printf("  size %03x STARTED, stopping\n", size);
				return;
			}
		}
		if (size == 0) {
			runResult = result;
		} else if (result != runResult || size == 0x801) {
			printf("  %03x-%03x: %s\n", runStart, size - 1, statusName(runResult));
			runStart = size;
			runResult = result;
		}
	}
	for (int i = 0; i < validCount; ++i) {
		printf("  (skipped %03x)\n", valid[i]);
	}
}

int main(int argc, char *argv[]) {
	static const unsigned int msgSizes[] = { 0x23C, 0x244, 0x2C4 };
	static const unsigned int netconfSizes[] = { 0x38, 0x40, 0x44 };
	static const unsigned int oskSizes[] = { 0x40, 0x44 };
	static const unsigned int npSizes[] = { 0x40 };
	static const unsigned int screenshotSizes[] = { 0x1B4, 0x3A0, 0x3A4 };
	static const unsigned int installSizes[] = { 0x590, 0x598 };
	static const unsigned int savedataSizes[] = { 0x5C8, 0x5DC, 0x600 };
	static const unsigned int sharingSizes[] = { 0x50, 0x54, 0x64 };

	sweep("msg", &msgInit, msgSizes, ARRAY_SIZE(msgSizes));
	sweep("netconf", &netconfInit, netconfSizes, ARRAY_SIZE(netconfSizes));
	sweep("osk", &oskInit, oskSizes, ARRAY_SIZE(oskSizes));
	sweep("npsignin", (InitStartFunc)&sceUtilityNpSigninInitStart, npSizes, ARRAY_SIZE(npSizes));
	sweep("screenshot", (InitStartFunc)&sceUtilityScreenshotInitStart, screenshotSizes, ARRAY_SIZE(screenshotSizes));
	sweep("gamedatainstall", (InitStartFunc)&sceUtilityGamedataInstallInitStart, installSizes, ARRAY_SIZE(installSizes));
	sweep("savedata", &savedataInit, savedataSizes, ARRAY_SIZE(savedataSizes));
	sweep("gamesharing", &sharingInit, sharingSizes, ARRAY_SIZE(sharingSizes));
	printAllStatus("After");
	return 0;
}
