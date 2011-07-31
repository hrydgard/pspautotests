#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pspdebug.h>
#include <pspkdebug.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/lock.h>
//#include "local.h"

PSP_MODULE_INFO("TESTMODULE", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(0);

int RUNNING_ON_EMULATOR = 0;

extern int main2(int argc, char *argv[]);

FILE stdout_back;

static int writeStdoutHook(struct _reent *ptr, void *cookie, const char *buf, int buf_len) {
	char temp[1024 + 1];

	if (buf_len > (sizeof(temp) - 1)) {
		assert(0);
		abort();
	} else {
		memcpy(temp, buf, buf_len);
		temp[buf_len] = 0;
		Kprintf("%s", temp);
		pspDebugScreenPrintf("%s", temp);
	}
	
	//return stdout_back._write(ptr, cookie, buf, buf_len);
	
	return buf_len;
}

void test_begin() {
	pspDebugScreenInit();

	//freopen("test.txt", "wb", stdout);
	if (RUNNING_ON_EMULATOR) {
		fclose(stdout);
		stdout = fmemopen(alloca(4), 4, "wb");
		stdout_back._write = stdout->_write;
		stdout->_write = writeStdoutHook;
	} else {
		//freopen("ms0:/__testoutput.txt", "wb", stdout);
		freopen("__testoutput.txt", "wb", stdout);
	}

	setvbuf(stdout, NULL, _IONBF, 0);
}

void test_end() {
	
}

#define START_WITH "ms0:/PSP/GAMES/virtual"

int main(int argc, char *argv[]) {
	int retval = 0;

	if (strncmp(argv[0], START_WITH, sizeof(START_WITH) - 1)) {
		RUNNING_ON_EMULATOR = 1;
	}

	test_begin();
	{
		printf("RUNNING_ON_EMULATOR: %s\n", RUNNING_ON_EMULATOR ? "yes" : "no");
		retval = main2(argc, argv);
	}
	test_end();
	
	return retval;
}
