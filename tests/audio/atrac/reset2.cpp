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

	// TODO: We should also test loading libatrac3plus.prx from some game directly from disk.
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_ATRAC3PLUS);


	// Module investigation

	/*
	SceUID moduleList[128];
	int count = 0;
	int retval = sceKernelGetModuleIdList(moduleList, sizeof(moduleList), &count);
	if (retval >= 0) {
		schedf("sceKernelGetModuleIdList: %d modules\n", count);
		for (int i = 0; i < count; i++) {
			SceKernelModuleInfo info;
			info.size = sizeof(info);
			retval = sceKernelQueryModuleInfo(moduleList[i], &info);
			if (retval >= 0) {
				schedf("  %d: %08x %s\n", i, moduleList[i], info.name);
				schedf("Name: %s Segments: %d Version: %d.%d:\n", info.name, info.nsegment, info.version[1], info.version[0]);
				int totalSize = 0;
				for (int i = 0; i < info.nsegment; i++) {
					schedf("  %d: %08x-%08x (size %d)\n", i, info.segmentaddr[i], info.segmentaddr[i] + info.segmentsize[i], info.segmentsize[i]);
					totalSize += info.segmentsize[i];
				}
				schedf("Entry: %08x GP: %08x totalSize: %d\n", info.entry_addr, info.gp_value, totalSize);
				schedf("Text: %08x-%08x Data size: %08x BSS size: %08x\n", info.text_addr, info.text_addr + info.text_size, info.data_size, info.bss_size);
			} else {
				schedf("  failed to query module %d: %08x\n", i, moduleList[i]);
			}
		}
	} else {
		schedf("sceKernelGetModuleIdList failed: %08x\n", retval);
	}
*/

	Atrac3File file("sample.at3");
	Atrac3File looped;
	CreateLoopedAtracFrom(file, looped, 2048, 249548);

	// This needs investigation. I think allocations happen at sceUtilityLoadModule.
	int atracID = sceAtracSetDataAndGetID(looped.Data(), 0x3000);
	if (atracID < 0) {
		printf("fail\n");
		return 1;
	}

	LogAtracContext(atracID, looped.Data(), NULL, true);

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
		LogAtracContext(atracID, looped.Data(), NULL, true);
	}

	sceAtracReleaseAtracID(atracID);
	return 0;
}
