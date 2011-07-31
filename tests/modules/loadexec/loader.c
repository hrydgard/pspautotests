#include <pspkernel.h>
#include <psploadexec.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/unistd.h>

PSP_MODULE_INFO("LOADER", 0x1000, 1, 1);

#define eprintf Kprintf

int main(int argc, char *argv[]) {
	char buf[MAXPATHLEN];
	struct SceKernelLoadExecParam params;
	params.size = sizeof(struct SceKernelLoadExecParam);
	params.args = argc;
	params.argp = argv;
	params.key  = NULL;

	getcwd(buf, MAXPATHLEN);
	strcat(buf, "/simple.prx");
	
	eprintf("[1]\n");
	sceKernelLoadExec(buf, &params);
	eprintf("[2]\n");

	return 0;
}