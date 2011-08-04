#include <common.h>

#include <pspkernel.h>
#include "sascore.h"

//PSP_MODULE_INFO("sascore test", 0, 1, 1);
//PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

int main(int argc, char *argv[]) {
	SasCore sasCore;
	int result = __sceSasInit(&sasCore, PSP_SAS_GRAIN_SAMPLES, PSP_SAS_VOICES_MAX, PSP_SAS_OUTPUTMODE_STEREO, 44100);
	printf("%d\n", result);

	return 0;
}