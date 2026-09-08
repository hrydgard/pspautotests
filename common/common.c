#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <pspdebug.h>
#include <pspthreadman.h>
#include <psploadexec.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspiofilemgr.h>
//#include <pspkdebug.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/lock.h>
#include <sys/fcntl.h>

//#include "local.h"

#include "sysmem-imports.h"

/*
enum PspThreadAttributes
{
	PSP_THREAD_ATTR_VFPU = 0x00004000,
	PSP_THREAD_ATTR_USER = 0x80000000,
	PSP_THREAD_ATTR_USBWLAN = 0xa0000000,
	PSP_THREAD_ATTR_VSH = 0xc0000000,
	PSP_THREAD_ATTR_SCRATCH_SRAM = 0x00008000,
	PSP_THREAD_ATTR_NO_FILLSTACK = 0x00100000,
	PSP_THREAD_ATTR_CLEAR_STACK = 0x00200000,
};
*/
/*
enum PspModuleInfoAttr
{
	PSP_MODULE_USER			= 0,
	PSP_MODULE_NO_STOP		= 0x0001,
	PSP_MODULE_SINGLE_LOAD	= 0x0002,
	PSP_MODULE_SINGLE_START	= 0x0004,
	PSP_MODULE_KERNEL		= 0x1000,
};
*/

#ifdef COMMON_KERNEL
PSP_MODULE_INFO("TESTMODULE", PSP_MODULE_KERNEL, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU);
#else
PSP_MODULE_INFO("TESTMODULE", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
#endif

#define EMULATOR_DEVCTL__GET_HAS_DISPLAY 0x00000001
#define EMULATOR_DEVCTL__SEND_OUTPUT     0x00000002
#define EMULATOR_DEVCTL__IS_EMULATOR     0x00000003
#define EMULATOR_DEVCTL__SEND_CTRLDATA   0x00000010
#define EMULATOR_DEVCTL__EMIT_SCREENSHOT 0x00000020

unsigned int RUNNING_ON_EMULATOR = 0;
unsigned int CHECKPOINT_ENABLE_TIME = 0;
unsigned int CHECKPOINT_OUTPUT_DIRECT = 0;
unsigned int HAS_DISPLAY = 1;

#ifdef COMMON_KERNEL
// A kernel module has to live in the kernel partition, which is a few hundred KB with PSPLink
// already resident - nothing like the 24 MB a user module gets. Both of these are sized so a
// kernel test actually loads; see docs/pspautotests-hardware.md.
#define SCHEDF_BUFFER_SIZE 4096
unsigned int sce_newlib_heap_kb_size = 64;
#else
#define SCHEDF_BUFFER_SIZE 65536
// 21 MB to give space for thread stacks and etc.
unsigned int sce_newlib_heap_kb_size = 21504;
#endif

extern int test_main(int argc, char *argv[]);

FILE stdout_back = {NULL};
//int KprintfFd = 0;

char schedfBuffer[SCHEDF_BUFFER_SIZE];
unsigned int schedfBufferPos = 0;

// Weak so that a test can supply its own schedf - a dozen of them do, to capture output into a
// buffer instead of printing it. Older linkers silently took the test's definition; modern ld
// rejects the duplicate unless this one is weak.
__attribute__((weak)) void schedf(const char *format, ...) {
	va_list args;
	va_start(args, format);
	if (CHECKPOINT_OUTPUT_DIRECT) {
		// This is easier to debug in the emulator, but printf() reschedules on the real PSP.
		vprintf(format, args);
	} else {
		schedfBufferPos += vsprintf(schedfBuffer + schedfBufferPos, format, args);
	}
	va_end(args);
}

#ifdef COMMON_KERNEL
// A kernel module can't use newlib's FILE layer: it pulls in libcglue, whose _open/_read/_write
// bring imports of sceNetInet and sceUtility along with them, and a kernel module that imports
// those fails to load with 8002013C. So talk to host0 through sceIo directly, and supply the
// printf that tests call rather than letting newlib's turn up and drag stdio back in.
static int kernelOutputFd = -1;

int printf(const char *format, ...) {
	static char line[2048];
	va_list args;
	va_start(args, format);
	int len = vsnprintf(line, sizeof(line), format, args);
	va_end(args);
	if (len < 0) {
		return len;
	}
	if (len > (int)sizeof(line) - 1) {
		len = (int)sizeof(line) - 1;
	}
	if (kernelOutputFd >= 0) {
		sceIoWrite(kernelOutputFd, line, len);
	}
	return len;
}
#endif

void flushschedf() {
	printf("%s", schedfBuffer);
	schedfBuffer[0] = '\0';
	schedfBufferPos = 0;
}

SceUID reschedThread;
volatile int didResched = 0;
int reschedFunc(SceSize argc, void *argp) {
	didResched = 1;
	return 0;
}

u64 lastCheckpoint = 0;
void checkpoint(const char *format, ...) {
	u64 currentCheckpoint = sceKernelGetSystemTimeWide();
	if (CHECKPOINT_ENABLE_TIME) {
		schedf("[%s/%lld] ", didResched ? "r" : "x", currentCheckpoint - lastCheckpoint);
	} else {
		schedf("[%s] ", didResched ? "r" : "x");
	}

	sceKernelTerminateThread(reschedThread);

	if (format != NULL) {
		va_list args;
		va_start(args, format);
		if (CHECKPOINT_OUTPUT_DIRECT) {
			// This is easier to debug in the emulator, but printf() reschedules on the real PSP.
			vprintf(format, args);
		} else {
			schedfBufferPos += vsprintf(schedfBuffer + schedfBufferPos, format, args);
		}
		va_end(args);
	}

	didResched = 0;
	sceKernelStartThread(reschedThread, 0, NULL);

	if (format != NULL) {
		schedf("\n");
	}
	
	lastCheckpoint = currentCheckpoint;
}

void checkpointNext(const char *title) {
	if (schedfBufferPos != 0) {
		schedf("\n");
	}
	flushschedf();
	didResched = 0;
	if (title != NULL)
		checkpoint(title);
}

static int writeStdoutHook(struct _reent *ptr, void *cookie, const char *buf, int buf_len) {
	char temp[1024 + 1];

	//if (KprintfFd > 0) sceIoWrite(KprintfFd, buf, buf_len);
	if (RUNNING_ON_EMULATOR) {
		sceIoDevctl("emulator:", EMULATOR_DEVCTL__SEND_OUTPUT, (void *)buf, buf_len, NULL, 0);
	}

#ifndef COMMON_KERNEL
	// Compiled out for kernel modules: the debug screen pulls in sceDisplay and sceGe, and a
	// kernel module that imports those fails to load. Output goes to host0 only there.
	if (buf_len < sizeof(temp)) {
		if (HAS_DISPLAY) {
			memcpy(temp, buf, buf_len);
			temp[buf_len] = 0;

			//Kprintf("%s", temp);
			pspDebugScreenPrintf("%s", temp);
		}
	}
#endif
	
	if (stdout_back._write != NULL) {
		return stdout_back._write(ptr, cookie, buf, buf_len);
	} else {
		return buf_len;
	}
}

#ifndef COMMON_KERNEL
typedef int (*SdkVerFunc)(int ver);
typedef struct SdkVerFuncTable {
	u32 id;
	SdkVerFunc func;
} SdkVerFuncTable;

static SdkVerFuncTable sdkVerFuncs[] = {
	{0, sceKernelSetCompiledSdkVersion},
	{370, sceKernelSetCompiledSdkVersion370},
	{380, sceKernelSetCompiledSdkVersion380_390},
	{395, sceKernelSetCompiledSdkVersion395},
	{401, sceKernelSetCompiledSdkVersion401_402},
	{500, sceKernelSetCompiledSdkVersion500_505},
	{507, sceKernelSetCompiledSdkVersion507},
	{600, sceKernelSetCompiledSdkVersion600_602},
	{603, sceKernelSetCompiledSdkVersion603_605},
	{606, sceKernelSetCompiledSdkVersion606},
};

static void updateSdkVer(int argc, char *argv[]) {
	int i = 0;
	u32 ver = 0xFFFFFFFF;
	u32 funcID = 0;
	for (i = 1; i < argc; ++i) {
		if (!strncmp(argv[i], "--sdkver=", strlen("--sdkver="))) {
			ver = strtoul(argv[i] + strlen("--sdkver="), NULL, 16);
		}
		if (!strncmp(argv[i], "--sdkver-func=", strlen("--sdkver-func="))) {
			funcID = strtol(argv[i] + strlen("--sdkver-func="), NULL, 10);
		}
	}

	SdkVerFunc func = NULL;
	for (i = 0; i < sizeof(sdkVerFuncs) / sizeof(sdkVerFuncs[0]); ++i) {
		if (sdkVerFuncs[i].id == funcID) {
			func = sdkVerFuncs[i].func;
		}
	}

	if (func == NULL) {
#ifndef COMMON_KERNEL
		fprintf(stderr, "Unknown sdkver-func value.\n");
#endif
		exit(1);
	}

	if (ver != 0xFFFFFFFF) {
		if (func(ver) != 0) {
			printf("WARNING: Setting sdkver returned failure.\n");
		}
	}
}
#endif

void test_begin() {
#ifndef COMMON_KERNEL
	if (HAS_DISPLAY) {
		pspDebugScreenInit();
	}
#endif

#ifdef COMMON_KERNEL
	kernelOutputFd = sceIoOpen("host0:/__testoutput.txt", PSP_O_CREAT | PSP_O_WRONLY | PSP_O_TRUNC, 0777);
#else
	if (RUNNING_ON_EMULATOR && !HAS_DISPLAY) {
		fclose(stdout);
		stdout = fmemopen(alloca(4), 4, "wb");
		stdout_back._write = NULL;
		
		//stderr = stdout;
		setbuf(stdout, NULL);
	} else {
    // Send the output to the host.
		freopen("host0:/__testoutput.txt", "wb", stdout);
		freopen("host0:/__testerror.txt", "wb", stderr);
		stdout_back._write = stdout->_write;
	}
	stdout->_write = writeStdoutHook;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	
	setbuf(stderr, NULL);
#endif

	reschedThread = sceKernelCreateThread("resched", &reschedFunc, sceKernelGetThreadCurrentPriority(), 0x1000, 0, NULL);
}

void test_end() {
	flushschedf();

#ifdef COMMON_KERNEL
	if (kernelOutputFd >= 0) {
		sceIoClose(kernelOutputFd);
		kernelOutputFd = -1;
	}
	if (!RUNNING_ON_EMULATOR) {
		int finish = sceIoOpen("host0:/__testfinish.txt", PSP_O_CREAT | PSP_O_WRONLY | PSP_O_TRUNC, 0777);
		if (finish >= 0) {
			sceIoWrite(finish, "1", 1);
			sceIoClose(finish);
		}
	}
#else
	fflush(stdout);
	fflush(stderr);
	
	fclose(stdout);
	fclose(stderr);

	if (!RUNNING_ON_EMULATOR) {
		FILE *finish = fopen("host0:/__testfinish.txt", "wb");
		if (finish)
		{
			fwrite("1", sizeof(char), 1, finish);
			fclose(finish);
		}
	}
#endif

  // Disabled the wait, much more convienent when running automated.
#ifndef COMMON_KERNEL
	if (0 && !RUNNING_ON_EMULATOR) {
		SceCtrlData key;
		while (1) {
			sceCtrlReadBufferPositive(&key, 1);
			if (key.Buttons & PSP_CTRL_CROSS) break;
		}
	}
#endif
	
	//fclose(stdout);
#ifndef COMMON_KERNEL
	sceKernelExitGame();
	
	exit(0);
#endif
}

#ifndef COMMON_KERNEL
// The exit callback is a game thing - sceKernelRegisterExitCallback lives in LoadExec, and a
// kernel module that imports it won't load. Compiled out rather than just left uncalled, since
// the reference alone is enough to pull the stub in.
int test_psp_exit_callback(int arg1, int arg2, void *common) {
	exit(0);
	return 0;
}

int test_psp_callback_thread(SceSize args, void *argp) {
	int cbid;
	cbid = sceKernelCreateCallback("Exit Callback", test_psp_exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);
	sceKernelSleepThreadCB();
	return 0;
}

int test_psp_setup_callbacks(void) {
	int thid = 0;
	thid = sceKernelCreateThread("update_thread",  test_psp_callback_thread, 0x11, 0xFA0, 0, 0);
	if (thid >= 0) sceKernelStartThread(thid, 0, 0);
	return thid;
}
#endif

//#define START_WITH "ms0:/PSP/GAME/virtual"

/*
void emitInt(int v) {
	asm("syscall 0x1010");
}

void emitFloat(float v) {
	asm("syscall 0x1011");
}

void emitString(char *v) {
	asm("syscall 0x1012");
}

void emitComment(char *v) {
	asm("syscall 0x1012");
}

void emitMemoryBlock(void *address, unsigned int size) {
	asm("syscall 0x1013");
}

void emitHex(void *address, unsigned int size) {
	asm("syscall 0x1014");
}
*/

unsigned char bmpHeader[54] = {
	0x42, 0x4D, 0x38, 0x80, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00,
	0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x10, 0x01,
	0x00, 0x00, 0x01, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x80,
	0x08, 0x00, 0x12, 0x0B, 0x00, 0x00, 0x12, 0x0B, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


static inline uint extractBits(uint value, int offset, int size) {
	return (value >> offset) & ((1 << size) - 1);
}

static inline uint extractExpand1Bits(uint value, int offset) {
	uint extracted = (value >> offset) & 1;
	return extracted ? 0xFF : 0;
}

static inline uint extractExpand4Bits(uint value, int offset) {
	uint extracted = (value >> offset) & ((1 << 4) - 1);
	return (extracted << 4) | (extracted >> 0);
}

static inline uint extractExpand5Bits(uint value, int offset) {
	uint extracted = (value >> offset) & ((1 << 5) - 1);
	return (extracted << 3) | (extracted >> 2);
}

static inline uint extractExpand6Bits(uint value, int offset) {
	uint extracted = (value >> offset) & ((1 << 6) - 1);
	return (extracted << 2) | (extracted >> 4);
}

static void rgab8888_to_bgra8888(uint *dst, const uint *src, int num) {
	int i;
	uint c;
	uint r, g, b, a;
	for (i = 0; i < num; i++) {
		c = src[i];
		r = extractBits(c,  0, 8);
		g = extractBits(c,  8, 8);
		b = extractBits(c, 16, 8);
		a = extractBits(c, 24, 8);
		dst[i] = (b << 0) | (g << 8) | (r << 16) | (a << 24);
	}
}

static void rgab4444_to_bgra8888(uint *dst, const ushort *src, int num) {
	int i;
	uint c;
	uint r, g, b, a;
	for (i = 0; i < num; i++) {
		c = src[i];
		r = extractExpand4Bits(c,  0);
		g = extractExpand4Bits(c,  4);
		b = extractExpand4Bits(c,  8);
		a = extractExpand4Bits(c, 12);
		dst[i] = (b << 0) | (g << 8) | (r << 16) | (a << 24);
	}
}

static void rgab5551_to_bgra8888(uint *dst, const ushort *src, int num) {
	int i;
	uint c;
	uint r, g, b, a;
	for (i = 0; i < num; i++) {
		c = src[i];
		r = extractExpand5Bits(c,  0);
		g = extractExpand5Bits(c,  5);
		b = extractExpand5Bits(c, 10);
		a = extractExpand1Bits(c, 15);
		dst[i] = (b << 0) | (g << 8) | (r << 16) | (a << 24);
	}
}

static void rgab565_to_bgra8888(uint *dst, const ushort *src, int num) {
	int i;
	uint c;
	uint r, g, b, a;
	for (i = 0; i < num; i++) {
		c = src[i];
		r = extractExpand5Bits(c,  0);
		g = extractExpand6Bits(c,  5);
		b = extractExpand5Bits(c, 11);
		a = 0xFF;
		dst[i] = (b << 0) | (g << 8) | (r << 16) | (a << 24);
	}
}

#ifdef COMMON_KERNEL
// Needs sceDisplay, which a kernel module can't import.
void emulatorEmitScreenshot() {
}
#else
void emulatorEmitScreenshot() {
	int file;

	if (RUNNING_ON_EMULATOR) {
		sceIoDevctl("kemulator:", EMULATOR_DEVCTL__EMIT_SCREENSHOT, NULL, 0, NULL, 0);
	}
	else
	{
		uint topaddr;
		int bufferwidth;
		int pixelformat;

		sceDisplayGetFrameBuf((void **)&topaddr, &bufferwidth, &pixelformat, 0);
		
        if (topaddr & 0x80000000) {
            topaddr |= 0xA0000000;
        } else {
            topaddr |= 0x40000000;
        }
	
		if ((file = sceIoOpen("host0:/__screenshot.bmp", PSP_O_CREAT | PSP_O_WRONLY | PSP_O_TRUNC, 0777)) >= 0) {
			int y;
			uint* vram_row;
			uint* row_buf = (uint *)malloc(512 * 4);
			uint row_bytes = (pixelformat == PSP_DISPLAY_PIXEL_FORMAT_8888 ? 4 : 2) * bufferwidth;
			sceIoWrite(file, &bmpHeader, sizeof(bmpHeader));
			for (y = 0; y < 272; y++) {
				vram_row = (uint *)(topaddr + row_bytes * (271 - y));
				if (pixelformat == PSP_DISPLAY_PIXEL_FORMAT_8888) {
					rgab8888_to_bgra8888(row_buf, vram_row, 512);
				} else if (pixelformat == PSP_DISPLAY_PIXEL_FORMAT_4444) {
					rgab4444_to_bgra8888(row_buf, (const ushort *)vram_row, 512);
				} else if (pixelformat == PSP_DISPLAY_PIXEL_FORMAT_5551) {
					rgab5551_to_bgra8888(row_buf, (const ushort *)vram_row, 512);
				} else if (pixelformat == PSP_DISPLAY_PIXEL_FORMAT_565) {
					rgab565_to_bgra8888(row_buf, (const ushort *)vram_row, 512);
				} else {
					printf("ERROR: Invalid format %d", pixelformat);
				}
				sceIoWrite(file, row_buf, 512 * 4);
			}
			free(row_buf);
			//sceIoWrite(file, (void *)topaddr, bufferwidth * 272 * 4);
			//sceIoFlush();
			sceIoClose(file);
		}
	}
}
#endif

void emulatorSendSceCtrlData(SceCtrlData* pad_data) {
	sceIoDevctl("kemulator:", EMULATOR_DEVCTL__SEND_CTRLDATA, pad_data, sizeof(SceCtrlData), NULL, 0);
}

int main(int argc, char *argv[]) {
	int retval = 0;

	//KprintfFd = sceIoOpen("emulator:/Kprintf", O_WRONLY, 0777);
	//RUNNING_ON_EMULATOR = (KprintfFd > 0);
	
	if (sceIoDevctl("kemulator:", EMULATOR_DEVCTL__IS_EMULATOR, NULL, 0, NULL, 0) == 0) {
		RUNNING_ON_EMULATOR = 1;
	}
	
	// TEMP HACK
	//RUNNING_ON_EMULATOR = 0;

	if (RUNNING_ON_EMULATOR) {
		sceIoDevctl("kemulator:", EMULATOR_DEVCTL__GET_HAS_DISPLAY, NULL, 0, &HAS_DISPLAY, sizeof(HAS_DISPLAY));
	}
	
	#ifdef GRAPHIC_TEST
		HAS_DISPLAY = 0;
	#endif

	//if (strncmp(argv[0], START_WITH, strlen(START_WITH)) == 0) RUNNING_ON_EMULATOR = 1;

#ifndef COMMON_KERNEL
	// Both of these are about being a game: the exit callback and sceKernelExitGame come from
	// LoadExec, which a kernel module shouldn't be importing (and doesn't need - it just
	// returns from module_start).
	if (!RUNNING_ON_EMULATOR) {
		test_psp_setup_callbacks();
	}
	atexit(sceKernelExitGame);
#endif

	test_begin();
	{
#ifndef COMMON_KERNEL
		pspDebugScreenPrintf("RUNNING_ON_EMULATOR: %s - %s\n", RUNNING_ON_EMULATOR ? "yes" : "no", argv[0]);
#endif
#ifndef COMMON_KERNEL
		// sceKernelSetCompiledSdkVersion lives in SysMemUserForUser, and a kernel module can't
		// import the ForUser libraries - it fails to load with 8002013C.
		updateSdkVer(argc, argv);
#endif

		retval = test_main(argc, argv);
	}
	test_end();
	
	return retval;
}
