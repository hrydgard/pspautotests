#include <common.h>
#include <pspiofilemgr.h>
extern "C" {
#include "sysmem-imports.h"
}

extern "C" int main(int argc, char *argv[]) {
	checkpointNext("Missing parent dir:");

	SceUID fd = sceIoOpen("ms0:/noexist/item.txt", PSP_O_CREAT | PSP_O_WRONLY, 0777);
	checkpoint("  Open ms0:/noexist/item.txt: %08x", fd);
	sceIoClose(fd);

	SceIoStat stat;
	int result = sceIoGetstat("ms0:/noexist/item.txt", &stat);
	checkpoint("  Stat ms0:/noexist/item.txt: %08x", result);

	result = sceIoRemove("ms0:/noexist/item.txt");
	checkpoint("  Remove ms0:/noexist/item.txt: %08x", result);

	// In case it was auto-created.
	sceIoRmdir("ms0:/noexist");

	return 0;
}