#include "emptyfilename.h"

extern "C" int main(int argc, char **argv) {
	initDisplay();

	runEmptyFileNameTrials("auto delete:", SCE_UTILITY_SAVEDATA_TYPE_AUTODELETE);
	runEmptyFileNameTrials("delete:", SCE_UTILITY_SAVEDATA_TYPE_DELETE);
	runEmptyFileNameTrials("list delete:", SCE_UTILITY_SAVEDATA_TYPE_LISTDELETE);
	runEmptyFileNameTrials("list all delete:", SCE_UTILITY_SAVEDATA_TYPE_LISTALLDELETE);
	runEmptyFileNameTrials("delete data:", SCE_UTILITY_SAVEDATA_TYPE_DELETEDATA);
	runEmptyFileNameTrials("erase secure:", SCE_UTILITY_SAVEDATA_TYPE_ERASESECURE);
	runEmptyFileNameTrials("erase:", SCE_UTILITY_SAVEDATA_TYPE_ERASE);

	return 0;
}