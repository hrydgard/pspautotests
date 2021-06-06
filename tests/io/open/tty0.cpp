#include <common.h>
#include <pspstdio.h>
#include <pspiofilemgr.h>

extern "C" int main(int argc, char *argv[]) {
	// Let's be fair... need to simulate the stderr/stdout pipes.
	if (RUNNING_ON_EMULATOR) {
		sceIoOpen("fds.prx", PSP_O_RDONLY, 0);
	}

	checkpointNext("sceIoOpen tty0:");

	checkpoint("  Read only: %08x", sceIoOpen("tty0:", PSP_O_RDONLY, 0));
	checkpoint("  Write only: %08x", sceIoOpen("tty0:", PSP_O_WRONLY, 0));
	checkpoint("  Read write: %08x", sceIoOpen("tty0:", PSP_O_RDWR, 0));
	checkpoint("  Create: %08x", sceIoOpen("tty0:", PSP_O_WRONLY | PSP_O_CREAT, 0));
	checkpoint("  Truncate: %08x", sceIoOpen("tty0:", PSP_O_WRONLY | PSP_O_TRUNC, 0));
	checkpoint("  Append: %08x", sceIoOpen("tty0:", PSP_O_WRONLY | PSP_O_APPEND, 0));
	checkpoint("  With name: %08x", sceIoOpen("tty0:foobar.txt", PSP_O_RDONLY, 0));
	checkpoint("  TTY1: %08x", sceIoOpen("tty1:", PSP_O_RDONLY, 0));

	int fd = sceIoOpen("tty0:", PSP_O_WRONLY, 0);
	sceIoWrite(fd, "test\n", 5);

	return 0;
}