#include <common.h>

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspatrac3.h>
#include <pspaudio.h>
#include <pspctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <psputility.h>

#include "atrac.h"
#include "shared.h"

// We log too much.
extern unsigned int CHECKPOINT_OUTPUT_DIRECT;
extern unsigned int HAS_DISPLAY;

extern "C" int main(int argc, char *argv[]) {
	CHECKPOINT_OUTPUT_DIRECT = 1;
	HAS_DISPLAY = 1;  // don't waste time logging to the screen.

	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_ATRAC3PLUS);

	Atrac3File file("sample.at3");
	Atrac3File looped;
	CreateLoopedAtracFrom(file, looped, 2048, 249548);

	// This needs investigation. I think allocations happen at sceUtilityLoadModule.
	int atracID = sceAtracSetDataAndGetID(looped.Data(), 0x3000);
	if (atracID < 0) {
		printf("fail\n");
		return 1;
	}

	LogAtracContext(atracID, (u32)(uintptr_t)looped.Data(), true);

	// printf("atrac ctx ptr: %p\n", _sceAtracGetContextAddress(atracID));

	AtracResetBufferInfo resetInfo;
	for (int i = 0; i < 3; i++) {
		int offset = 2048 + i * 500;
		memset(&resetInfo, 0xCC, sizeof(resetInfo));
		int result = sceAtracGetBufferInfoForResetting(atracID, offset, &resetInfo);
		schedf("%08x: Reset info for sample %d:\n", result, offset);
		schedfResetBuffer(resetInfo, file.Data());

		// Perform the read.
		if (result >= 0 && resetInfo.first.writableBytes > 0) {
			file.Seek(resetInfo.first.filePos, SEEK_SET);
			int readSize = resetInfo.first.writableBytes;
			int bytesRead = file.Read(resetInfo.first.writePos, readSize);
			schedf("Read %d/%d bytes from %d in file\n", bytesRead, readSize, resetInfo.first.filePos);
			sceAtracResetPlayPosition(atracID, offset, bytesRead, 0);
			schedf("%08x=sceAtracResetPlayPosition(%d, %d, %d)\n", result, atracID, offset, bytesRead);
		} else if (result < 0) {
			schedf("Failed to get reset info for sample %d\n", offset);
		}
		LogAtracContext(atracID, (u32)(uintptr_t)looped.Data(), true);
	}

	sceAtracReleaseAtracID(atracID);
	return 0;
}
