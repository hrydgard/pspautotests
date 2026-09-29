#define sceUtilityMsgDialogShutdownStart sceUtilityMsgDialogShutdownStart_WRONG
#define sceUtilityMsgDialogUpdate sceUtilityMsgDialogUpdate_WRONG

#include <common.h>
#include <pspgu.h>
#include <pspdisplay.h>
#include <psputility.h>
#include <psputility_msgdialog.h>
#include <pspkernel.h>

#undef sceUtilityMsgDialogShutdownStart
#undef sceUtilityMsgDialogUpdate

// While a dialog starts up and shuts down, its threads (here all at 0x28) work in the background.
// Does a thread just below them (0x30) get the CPU meanwhile? If they're busy computing it
// doesn't; if they're waiting (on flash, say) it does. Main (0x20) sleeps on vblank throughout.

typedef struct {
	pspUtilityDialogCommon base;
	int result;
	int mode;
	int errorNum;
	char message[512];
	u32 options;
	u32 buttonPressed;
	char okayButton[64];
	char cancelButton[64];
} pspUtilityMsgDialogParamsV3;

extern "C" {
	int sceUtilityMsgDialogUpdate(int n);
	int sceUtilityMsgDialogShutdownStart();
}

static unsigned int __attribute__((aligned(16))) list[262144];
static volatile int stopProbe = 0;
static volatile u32 probeSpins = 0;

static int probeThread(SceSize argc, void *argp) {
	while (!stopProbe) {
		probeSpins++;
	}
	return 0;
}

static const char *shareBucket(u32 spins, u32 spinsPerMs, u32 us) {
	u32 possible = spinsPerMs * us / 1000;
	if (possible == 0) {
		return "?";
	}
	u32 percent = (u32)((u64)spins * 100 / possible);
	if (percent < 10) {
		return "under 10%";
	} else if (percent < 50) {
		return "10-50%";
	} else if (percent < 90) {
		return "50-90%";
	}
	return "over 90%";
}

extern "C" int main(int argc, char *argv[]) {
	sceGuInit();
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888, 0, 512);
	sceGuDispBuffer(480, 272, 0, 512);
	sceGuScissor(0, 0, 480, 272);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFinish();
	sceGuSync(0, 0);
	sceDisplaySetMode(0, 480, 272);
	sceDisplayWaitVblankStart();
	sceGuDisplay(1);

	pspUtilityMsgDialogParamsV3 dialog;
	memset(&dialog, 0, sizeof(dialog));
	dialog.base.size = sizeof(dialog);
	dialog.base.language = 1;
	dialog.base.buttonSwap = 1;
	dialog.base.soundThread = 0x28;
	dialog.base.graphicsThread = 0x28;
	dialog.base.accessThread = 0x28;
	dialog.base.fontThread = 0x28;
	dialog.mode = PSP_UTILITY_MSGDIALOG_MODE_TEXT;
	strcpy(dialog.message, "Test");

	// How fast the probe spins with the CPU to itself.
	SceUID probe = sceKernelCreateThread("probe", &probeThread, 0x30, 0x1000, 0, NULL);
	sceKernelStartThread(probe, 0, NULL);
	u32 before = probeSpins;
	sceKernelDelayThread(20000);
	u32 spinsPerMs = (probeSpins - before) / 20;

	int frame = 0;
	u32 initUsRounds[2], initSpinsRounds[2], shutdownUsRounds[2], shutdownSpinsRounds[2];
	int initResult = 0, shutdownResult = 0;
	for (int round = 0; round < 2; ++round) {
		frame = 0;
		u32 spinsStart = probeSpins;
		u32 start = sceKernelGetSystemTimeLow();
		initResult = sceUtilityMsgDialogInitStart((pspUtilityMsgDialogParams *)&dialog);
		while (sceUtilityMsgDialogGetStatus() == 1 && frame++ < 600) {
			sceDisplayWaitVblankStart();
		}
		u32 initUs = sceKernelGetSystemTimeLow() - start;
		u32 initSpins = probeSpins - spinsStart;

		// Let it draw a little, then close it.
		for (int i = 0; i < 10 && sceUtilityMsgDialogGetStatus() == 2; ++i) {
			sceUtilityMsgDialogUpdate(1);
			sceDisplayWaitVblankStart();
		}
		sceUtilityMsgDialogAbort();
		frame = 0;
		while ((u32)sceUtilityMsgDialogGetStatus() < 3 && frame++ < 600) {
			sceUtilityMsgDialogUpdate(1);
			sceDisplayWaitVblankStart();
		}

		spinsStart = probeSpins;
		start = sceKernelGetSystemTimeLow();
		shutdownResult = sceUtilityMsgDialogShutdownStart();
		frame = 0;
		while (sceUtilityMsgDialogGetStatus() != 0 && frame++ < 600) {
			sceDisplayWaitVblankStart();
		}
		u32 shutdownUs = sceKernelGetSystemTimeLow() - start;
		u32 shutdownSpins = probeSpins - spinsStart;

		initUsRounds[round] = initUs;
		initSpinsRounds[round] = initSpins;
		shutdownUsRounds[round] = shutdownUs;
		shutdownSpinsRounds[round] = shutdownSpins;
	}
	// Stop it first: it would starve the output going to the host.
	stopProbe = 1;
	sceKernelTerminateDeleteThread(probe);

	checkpointNext("Worse thread's share of the CPU:");
	for (int round = 0; round < 2; ++round) {
		checkpoint("  Dialog %d, while starting up: %s, while shutting down: %s", round + 1, shareBucket(initSpinsRounds[round], spinsPerMs, initUsRounds[round]), shareBucket(shutdownSpinsRounds[round], spinsPerMs, shutdownUsRounds[round]));
		checkpoint("    (starting up took ~%dms, shutting down ~%dms)", (initUsRounds[round] + 25000) / 50000 * 50, (shutdownUsRounds[round] + 5000) / 10000 * 10);
	}
	checkpoint("  InitStart: %08x, ShutdownStart: %08x", initResult, shutdownResult);
	checkpoint("  Final status: %d", sceUtilityMsgDialogGetStatus());
	return 0;
}
