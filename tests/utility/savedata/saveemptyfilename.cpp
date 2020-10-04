#include "emptyfilename.h"

extern "C" int main(int argc, char **argv) {
	initDisplay();

	runEmptyFileNameTrials("auto save", SCE_UTILITY_SAVEDATA_TYPE_AUTOSAVE, 0);
	runEmptyFileNameTrials("save:", SCE_UTILITY_SAVEDATA_TYPE_SAVE, 0);
	runEmptyFileNameTrials("list save:", SCE_UTILITY_SAVEDATA_TYPE_LISTSAVE, 0);
	runEmptyFileNameTrials("write data secure:", SCE_UTILITY_SAVEDATA_TYPE_WRITEDATASECURE, 0);
	runEmptyFileNameTrials("write data:", SCE_UTILITY_SAVEDATA_TYPE_WRITEDATA, 0);
	runEmptyFileNameTrials("make data secure:", SCE_UTILITY_SAVEDATA_TYPE_MAKEDATASECURE, 0);
	runEmptyFileNameTrials("make data:", SCE_UTILITY_SAVEDATA_TYPE_MAKEDATA, 0);

	return 0;
}