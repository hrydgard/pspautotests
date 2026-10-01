#include <common.h>

#include <pspthreadman.h>
#include <pspintrman.h>
#include <pspiofilemgr.h>
#include <psputility.h>
#include <pspmpeg.h>
#include <malloc.h>
#include <string.h>

#include "../../audio/sascore/sascore.h"

// Calls that take a while on hardware, made where nothing can wait: inside an interrupt handler,
// and with interrupts or dispatch disabled.

int sceAtracReinit(int at3origCount, int at3plusCount);
// The SDK declares it void.
static int (*const mpegDelete)(SceMpeg *) = (void *)&sceMpegDelete;

__attribute__((aligned(64))) SasCore sasCore;
__attribute__((aligned(64))) short sasOut[256 * 2];

// Bigger than any version of the struct.
__attribute__((aligned(64))) char ringbuffer[256];
void *ringbufferData;
int ringbufferSize;
void *mpegData;
int mpegSize;
SceMpeg mpeg;

static int norm(int result) {
	// File descriptors differ between runs.
	return result >= 0 ? 0 : result;
}

static void runAll(const char *where, int all) {
	int fd = sceIoOpen("host0:/tests/intr/delays/delays.c", PSP_O_RDONLY, 0);
	schedf("  %s: sceIoOpen: %08x\n", where, norm(fd));
	if (fd >= 0) {
		schedf("  %s: sceIoClose: %08x\n", where, sceIoClose(fd));
	}

	schedf("  %s: __sceSasCore: %08x\n", where, __sceSasCore(&sasCore, sasOut));

	if (!all) {
		return;
	}

	schedf("  %s: sceAtracReinit(0, 0): %08x\n", where, sceAtracReinit(0, 0));
	schedf("  %s: sceAtracReinit(2, 2): %08x\n", where, sceAtracReinit(2, 2));

	int result = sceMpegCreate(&mpeg, mpegData, mpegSize, (SceMpegRingbuffer *)ringbuffer, 512, 0, 0);
	schedf("  %s: sceMpegCreate: %08x\n", where, result);
	if (result == 0) {
		schedf("  %s: sceMpegDelete: %08x\n", where, mpegDelete(&mpeg));
	}
}

// Undoes whatever a failed call above left behind, so each context starts from the same state.
static void reset() {
	printf("  Reset: sceAtracReinit(0, 0): %08x\n", sceAtracReinit(0, 0));
	printf("  Reset: sceAtracReinit(2, 2): %08x\n", sceAtracReinit(2, 2));

	sceMpegRingbufferDestruct((SceMpegRingbuffer *)ringbuffer);
	sceMpegFinish();
	printf("  Reset: sceMpegInit: %08x\n", (int)sceMpegInit());
	memset(ringbuffer, 0, sizeof(ringbuffer));
	sceMpegRingbufferConstruct((SceMpegRingbuffer *)ringbuffer, 32, ringbufferData, ringbufferSize, NULL, NULL);
	free(mpegData);
	mpegData = memalign(64, mpegSize);
	memset(mpegData, 0, mpegSize);
	memset(&mpeg, 0, sizeof(mpeg));
	int result = sceMpegCreate(&mpeg, mpegData, mpegSize, (SceMpegRingbuffer *)ringbuffer, 512, 0, 0);
	printf("  Reset: sceMpegCreate: %08x\n", result);
	if (result == 0) {
		printf("  Reset: sceMpegDelete: %08x\n", mpegDelete(&mpeg));
	}
	fflush(stdout);
}

static void flush() {
	flushschedf();
	fflush(stdout);
}

static void section(const char *title) {
	flush();
	checkpointNext(title);
	flush();
	reset();
}

volatile int intrRan = 0;

void interruptFunc(int no, void *arg) {
	if (intrRan) {
		return;
	}
	intrRan = 1;
	runAll("Interrupt", 1);
}

int main(int argc, char *argv[]) {
	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_ATRAC3PLUS);
	sceUtilityLoadModule(PSP_MODULE_AV_SASCORE);
	sceUtilityLoadModule(PSP_MODULE_AV_MPEGBASE);

	__sceSasInit(&sasCore, 256, 32, 0, 44100);

	sceMpegInit();
	ringbufferSize = sceMpegRingbufferQueryMemSize(32);
	ringbufferData = memalign(64, ringbufferSize);
	mpegSize = sceMpegQueryMemSize(0);
	sceMpegRingbufferConstruct((SceMpegRingbuffer *)ringbuffer, 32, ringbufferData, ringbufferSize, NULL, NULL);

	// Flushed as we go, so a hang shows how far the test got.
	section("Normal:");
	runAll("Normal", 1);

	section("Inside interrupt:");
	sceKernelRegisterSubIntrHandler(PSP_VBLANK_INT, 1, (void *)interruptFunc, NULL);
	sceKernelEnableSubIntr(PSP_VBLANK_INT, 1);
	sceKernelDelayThread(100000);
	sceKernelDisableSubIntr(PSP_VBLANK_INT, 1);
	sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, 1);
	if (!intrRan) {
		schedf("  Interrupt didn't run\n");
	}

	// Atrac and Mpeg are left out here. A sceMpegCreate that fails with interrupts or dispatch
	// disabled leaves the library unusable (even sceMpegInit then fails with ALREADY_INIT), and so
	// does a sceAtracReinit that fails with interrupts disabled (sceMpegCreate then fails with
	// 80628001), so the results would depend on what ran before.
	section("Interrupts disabled:");
	int state = sceKernelCpuSuspendIntr();
	runAll("Interrupts disabled", 0);
	sceKernelCpuResumeIntr(state);

	section("Dispatch disabled:");
	state = sceKernelSuspendDispatchThread();
	runAll("Dispatch disabled", 0);
	sceKernelResumeDispatchThread(state);
	flush();

	sceMpegRingbufferDestruct((SceMpegRingbuffer *)ringbuffer);
	sceMpegFinish();
	free(mpegData);
	free(ringbufferData);
	return 0;
}
