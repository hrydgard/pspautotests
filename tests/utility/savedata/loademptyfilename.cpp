#include "emptyfilename.h"

extern "C" int main(int argc, char **argv) {
	initDisplay();

	runEmptyFileNameTrials("auto load:", SCE_UTILITY_SAVEDATA_TYPE_AUTOLOAD);
	runEmptyFileNameTrials("load:", SCE_UTILITY_SAVEDATA_TYPE_LOAD);
	runEmptyFileNameTrials("list load:", SCE_UTILITY_SAVEDATA_TYPE_LISTLOAD);
	runEmptyFileNameTrials("read data secure:", SCE_UTILITY_SAVEDATA_TYPE_READDATASECURE);
	runEmptyFileNameTrials("read data:", SCE_UTILITY_SAVEDATA_TYPE_READDATA);

	return 0;
}