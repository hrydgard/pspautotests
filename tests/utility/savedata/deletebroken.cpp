#include "broken.h"

extern "C" int main(int argc, char **argv) {
	initDisplay();

	for (int i = 1; i <= 3; i++) {
		runBrokenTrials("auto delete", SCE_UTILITY_SAVEDATA_TYPE_AUTODELETE, i);
		runBrokenTrials("delete", SCE_UTILITY_SAVEDATA_TYPE_DELETE, i);
		runBrokenTrials("list delete", SCE_UTILITY_SAVEDATA_TYPE_LISTDELETE, i);
		runBrokenTrials("list all delete", SCE_UTILITY_SAVEDATA_TYPE_LISTALLDELETE, i);
		runBrokenTrials("delete data", SCE_UTILITY_SAVEDATA_TYPE_DELETEDATA, i);
		runBrokenTrials("erase secure", SCE_UTILITY_SAVEDATA_TYPE_ERASESECURE, i);
		runBrokenTrials("erase", SCE_UTILITY_SAVEDATA_TYPE_ERASE, i);
	}

	return 0;
}