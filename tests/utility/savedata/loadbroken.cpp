#include "broken.h"

extern "C" int main(int argc, char **argv) {
	initDisplay();

	for (int i = 1; i <= 3; i++) {
		runBrokenTrials("auto load", SCE_UTILITY_SAVEDATA_TYPE_AUTOLOAD, i);
		runBrokenTrials("load", SCE_UTILITY_SAVEDATA_TYPE_LOAD, i);
		runBrokenTrials("list load", SCE_UTILITY_SAVEDATA_TYPE_LISTLOAD, i);
		runBrokenTrials("read data secure", SCE_UTILITY_SAVEDATA_TYPE_READDATASECURE, i);
		runBrokenTrials("read data", SCE_UTILITY_SAVEDATA_TYPE_READDATA, i);
	}

	return 0;
}