#include <common.h>

#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pspsdk.h>
#include <psputility.h>
#include "sascore.h"

//PSP_MODULE_INFO("sascore test", 0, 1, 1);
//PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define ARRAY_SIZE(a) (sizeof((a)) / (sizeof((a)[0])))

static char schedulingLog[65536];
static volatile int schedulingLogPos = 0;

inline void schedf(const char *format, ...) {
	va_list args;
	va_start(args, format);
	schedulingLogPos += vsprintf(schedulingLog + schedulingLogPos, format, args);
	// This is easier to debug in the emulator, but printf() reschedules on the real PSP.
	//vprintf(format, args);
	va_end(args);
}

inline void flushschedf() {
	printf("%s", schedulingLog);
	schedulingLogPos = 0;
}

SceUID reschedThread;
volatile int didResched = 0;
int reschedFunc(SceSize argc, void *argp) {
	didResched = 1;
	return 0;
}

//#define ENABLE_TIMESTAMP_CHECKPOINTS
u64 lastCheckpoint = 0;
void checkpoint(const char *format, ...) {
#ifdef ENABLE_TIMESTAMP_CHECKPOINTS
	u32 currentCheckpoint = sceKernelGetSystemTimeWide();
	schedf("[%s/%lld] ", didResched ? "r" : "x", currentCheckpoint - lastCheckpoint);
#else
	schedf("[%s] ", didResched ? "r" : "x");
#endif

	sceKernelTerminateThread(reschedThread);

	va_list args;
	va_start(args, format);
	schedulingLogPos += vsprintf(schedulingLog + schedulingLogPos, format, args);
	// This is easier to debug in the emulator, but printf() reschedules on the real PSP.
	//vprintf(format, args);
	va_end(args);

	didResched = 0;
	sceKernelStartThread(reschedThread, 0, NULL);

	schedf("\n");

#ifdef ENABLE_TIMESTAMP_CHECKPOINTS
	lastCheckpoint = currentCheckpoint;
#endif
}

void checkpointNext(const char *title) {
	if (schedulingLogPos != 0) {
		schedf("\n");
	}
	flushschedf();
	didResched = 0;
	checkpoint(title);
}


typedef struct {
	unsigned char* pointer;
	int length;
} ByteArray;

ByteArray loadData()
{
	int n;
	FILE *file;
	ByteArray data = {0};
	if ((file = fopen("test.vag", "rb")) != NULL) {
		fseek(file, 0, SEEK_END);
		data.length = ftell(file);
		data.pointer = (unsigned char *)malloc(data.length);
		memset(data.pointer, 0, data.length);
		fseek(file, 0, SEEK_SET);
		fread(data.pointer, data.length, 1, file);
		fclose(file);
	}
	if (data.length == 0) {
		printf("DATA:Can't read file\n");
	} else {
		printf("DATA:");
		for (n = 0; n < 0x20; n++) printf("%02X", data.pointer[n]);
		printf("\n");
	}
	return data;
}

// PSP_SAS_ERROR_ADDRESS = 0x80420005;
__attribute__((aligned(64))) SasCore sasCore;

__attribute__((aligned(64))) short samples[256 * 2 * 16] = {0};
ByteArray data;
int result;

int sasCoreThread(SceSize argsize, void *argdata) {
	int sasVoice = 0;
	int n, m;
	SasCore sasCoreInvalidAddress;

	printf("__sceSasInit         : 0x%08X\n", result = __sceSasInit(&sasCoreInvalidAddress, PSP_SAS_GRAIN_SAMPLES, PSP_SAS_VOICES_MAX, PSP_SAS_OUTPUTMODE_STEREO, 44100));
	printf("__sceSasInit         : 0x%08X\n", result = __sceSasInit(&sasCore, PSP_SAS_GRAIN_SAMPLES, PSP_SAS_VOICES_MAX, PSP_SAS_OUTPUTMODE_STEREO, 44100));
	printf("__sceSasSetOutputmode: 0x%08X\n", result = __sceSasSetOutputmode(&sasCore, PSP_SAS_OUTPUTMODE_STEREO));
	printf("__sceSasSetKeyOn     : 0x%08X\n", result = __sceSasSetKeyOn(&sasCore, sasVoice));
	printf("__sceSasSetVoice     : 0x%08X\n", result = __sceSasSetVoice(&sasCore, sasVoice, (char *)data.pointer, data.length, 0));
	printf("__sceSasSetPitch     : 0x%08X\n", result = __sceSasSetPitch(&sasCore, sasVoice, 4096));
	printf("__sceSasSetVolume    : 0x%08X\n", result = __sceSasSetVolume(&sasCore, sasVoice, 0x1000, 0x1000, 0x1000, 0x1000));
	printf("Data(%d):\n", data.length);
	
	//for (m = 0; m < 10000; m++) printf("%d,", data.pointer[m]);
	
	//return 0;
	
	for (m = 0; m < 6; m++) {
		printf("__sceSasCore: 0x%08X\n", result = __sceSasCore(&sasCore, samples));
		for (n = 0; n < 512; n++) printf("%d,", samples[n]);
		printf("\n");
	}
	printf("End\n");
	
	fflush(stdout);
	
	return 0;
}

void testSetVolumes(int voice, int l, int r, int el, int er) {
	int vol;
	for (vol = -0x1005; vol < 0x1005; vol++) {
		int result = __sceSasSetVolume(&sasCore, voice, l ? vol : 0, r ? vol : 0, el ? vol : 0, er ? vol : 0);
		if (result != 0)
			checkpoint("  Volumes: %d, %d, %d, %d: %08x", l ? vol : 0, r ? vol : 0, el ? vol : 0, er ? vol : 0, result);
	}
}

// http://www.psp-programming.com/forums/index.php?action=printpage;topic=4404.0
int main(int argc, char *argv[]) {
	int i;

	checkpoint("Load avcodec: %08x", sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC));
	checkpoint("Load sascore: %08x", sceUtilityLoadModule(PSP_MODULE_AV_SASCORE));

	// TODO: Not required to be called?  What does sasCore look like afterward?
	checkpointNext("sceSasInit:");
	checkpoint("  NULL: %08x", __sceSasInit(NULL, 1024, 32, 0, 44100));
	checkpoint("  Unaligned: %08x", __sceSasInit((SasCore *)((char *)&sasCore + 1), 1024, 32, 0, 44100));
	checkpoint("  Basic: %08x", __sceSasInit(&sasCore, 1024, 32, 0, 44100));
	checkpoint("  Twice: %08x", __sceSasInit(&sasCore, 1024, 32, 0, 44100));
	
	checkpointNext("  Grains:");
	int grains[] = {-0x400, -1, 0, 1, 2, 3, 4, 6, 8, 16, 32, 64, 92, 256, 512, 1024, 0x700, 0x7C0, 0x800, 0xC00, 0x1000, 0x100000};
	for (i = 0; i < ARRAY_SIZE(grains); ++i) {
		checkpoint("    %x: %08x", grains[i], __sceSasInit(&sasCore, grains[i], 32, 0, 44100));
	}
	
	checkpointNext("  Voices:");
	int voices[] = {-16, -1, 0, 1, 16, 31, 32, 33};
	for (i = 0; i < ARRAY_SIZE(voices); ++i) {
		checkpoint("    %d: %08x", voices[i], __sceSasInit(&sasCore, 1024, voices[i], 0, 44100));
	}

	checkpointNext("  Output modes:");
	int outModes[] = {-2, -1, 0, 1, 2, 3, 4};
	for (i = 0; i < ARRAY_SIZE(outModes); ++i) {
		checkpoint("    %d: %08x", outModes[i], __sceSasInit(&sasCore, 1024, 32, outModes[i], 44100));
	}

	checkpointNext("  Sample rates:");
	int sampleRates[] = {-1, 0, 1, 2, 3, 100, 3600, 4000, 8000, 11025, 16000, 22000, 22050, 22100, 32000, 44100, 48000, 48001, 64000, 88200, 96000};
	for (i = 0; i < ARRAY_SIZE(sampleRates); ++i) {
		checkpoint("    %d: %08x", sampleRates[i], __sceSasInit(&sasCore, 1024, 32, 0, sampleRates[i]));
	}
	
#if 0
	checkpointNext("Internal structure:");
	memset(&sasCore, 0xcc, sizeof(sasCore));
	__sceSasInit(&sasCore, 0x40, 1, 1, 44100);
	schedf("  H:", i);
	for (i = 0; i < ARRAY_SIZE(sasCore.header); i++) {
		schedf(" %x", sasCore.header[i]);
	}
	schedf("\n");

	for (i = 0; i < ARRAY_SIZE(sasCore.data); i += 14) {
		int j;
		schedf("%3d:", i);
		for (j = 0; j < 14; ++j)
			schedf(" %x", sasCore.data[i + j]);
		schedf("\n");
	}

	schedf("  F:", i);
	for (i = 0; i < ARRAY_SIZE(sasCore.footer); i++) {
		schedf(" %x", sasCore.footer[i]);
	}
	schedf("\n");
#endif

	checkpointNext("sceSasSetVolume:");
	testSetVolumes(0, 1, 0, 0, 0);
	testSetVolumes(0, 0, 1, 0, 0);
	testSetVolumes(0, 0, 0, 1, 0);
	testSetVolumes(0, 0, 0, 0, 1);
	testSetVolumes(0, 1, 1, 1, 1);

	for (i = 0; i < ARRAY_SIZE(voices); ++i) {
		checkpoint("  Voice: %d: %08x", voices[i], __sceSasSetVolume(&sasCore, voices[i], 0, 0, 0, 0));
	}

	// TODO: data = loadData();

	flushschedf();
	return 0;
}
