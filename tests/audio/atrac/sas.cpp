// NOTE: This test will play back choppily, you're not suppose to interleave audio and sascore on the
// same thread, but we do it for testing simplicity.

#include <common.h>
#include <psputility.h>
#include <pspctrl.h>
#include <pspatrac3.h>
#include <pspintrman.h>
#include <pspaudio.h>

#include "../sascore/sascore.h"
#include "atrac.h"
#include "shared.h"

__attribute__((aligned(64))) SasCore sasCore;
__attribute__((aligned(64))) short samples[4096 * 2 * 16] = {0};

extern unsigned int CHECKPOINT_OUTPUT_DIRECT;
extern unsigned int HAS_DISPLAY;

void LogSasChannel(int channel) {
	const SasVoice &voice = sasCore.voices[channel];
	schedf("Voice %d:\n", channel);
	schedf("params[0,1]=%08x, %08x\n", voice.unkNone[0], voice.unkNone[1]);
	schedf("type=%d, loop=%d, pitch=%d, leftVolume=%d, rightVolume=%d\n", voice.type, voice.loop, voice.pitch, voice.leftVolume, voice.rightVolume);
	schedf("effectLeftVolume=%d, effectRightVolume=%d, unk1=%08x\n", voice.effectLeftVolume, voice.effectRightVolume, voice.unk1);
	schedf("attack=%d,decay=%d,sustain=%08x,release=%08x\n", voice.attackRate, voice.decayRate, voice.sustainRate, voice.releaseRate);
	schedf("sustainLevel=%d, attackType=%d, decayType=%d, sustainType=%d, releaseType=%d\n", voice.sustainLevel, voice.attackType, voice.decayType, voice.sustainType, voice.releaseType);
	schedf("unk2=%04x, unk3=%02x, phase=%d, height=%d\n", voice.unk2, voice.unk3, voice.phase, voice.height);
}

extern "C" int main(int argc, char *argv[]) {
	CHECKPOINT_OUTPUT_DIRECT = 1;
	HAS_DISPLAY = 0;

	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_SASCORE);
	sceUtilityLoadModule(PSP_MODULE_AV_ATRAC3PLUS);

    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	Atrac3File at3("test_mono.at3");
	at3.Require();

	int atracID = sceAtracSetDataAndGetID(at3.Data(), at3.Size());
	if (atracID < 0) {
		schedf("sceAtracSetDataAndGetID: Failed %08x\n", atracID);
		return 1;
	} else {
		schedf("sceAtracSetDataAndGetID(..., %d): OK\n", at3.Size());
	}
	LogAtracContext(atracID, at3.Data(), NULL, true);

	const int grainSize = 1024;
	const int grainSampleBytes = grainSize * 4;

	int retval = __sceSasInit(&sasCore, grainSize, 32, 1, 44100);
	schedf("sceSasInit: %08x\n", retval);

	SceAtracId *ctx = (SceAtracId *)_sceAtracGetContextAddress(atracID);
	schedf("_sceAtracGetContextAddress: ...   streamOff=%08x\n", ctx->info.streamOff);

	int voice = 30;

	retval = __sceSasSetKeyOff(&sasCore, voice);
	schedf("__sceSasSetKeyOff: %08x\n", retval);

	retval = __sceSasSetOutputmode(&sasCore, 0);
	schedf("__sceSasSetOutputmode: %08x\n", retval);


	//retval = __sceSasSetADSRmode(&sasCore, voice, 15, PSP_SAS_ADSR_CURVE_MODE_LINEAR_INCREASE, PSP_SAS_ADSR_CURVE_MODE_DIRECT, PSP_SAS_ADSR_CURVE_MODE_DIRECT, PSP_SAS_ADSR_CURVE_MODE_DIRECT);
	//schedf("__sceSasSetADSRmode: %08x\n", retval);


	// Hacking the context
	// ===================
	// For Atrac3 channels to play correctly on Sas, we need to adjust the context a bit - seems
	// there's a completely different playback routine internally, that partially reuses the context struct.
	// * No variables in the Atrac3c context seem to get incremented, state is likely kept inside the sceSas channel struct.
	// * The second buffer is not used, so we'll set it to 0xFFFFFFFF.
	// * The loopNum is set to the voice number for unknown reasons, maybe just validation.
	// * Only AT3 is supported, not AT3+! AT3+ tracks are rejected by the wrapper in Sol Trigger.
	// * Only states 2-4 are supported, though I don't know how streaming will work.
	// * Only mono is supported.
	// * While modifying the context, we need to suspend interrupts.
	// * Codec inbuf/outbuf are constant
	// * I can't find the damn pointers being incremented!

	ctx->info.state = 0x10;
	retval = __sceSasSetVoiceATRAC3(&sasCore, voice, ctx);
	schedf("__sceSasSetVoiceATRAC3: %08x\n", retval);

	LogAtracContext(atracID, at3.Data(), NULL, true);

	ctx->info.buffer += ctx->info.streamOff;
	ctx->info.loopNum = voice;
	ctx->info.curFileOff += ctx->info.streamDataByte;
	ctx->info.secondBuffer = (u8 *)0xFFFFFFFF;

	LogAtracContext(atracID, at3.Data(), NULL, true);
	// LogSasChannel(voice);

	retval = __sceSasSetADSR(&sasCore,voice,0xf,0x40000000,0,0,0x10000);
	schedf("__sceSasSetADSR: %08x\n", retval);

	retval = __sceSasSetPitch(&sasCore, voice, 4096);

    int temp = sceKernelCpuSuspendIntr();

	sceKernelCpuResumeIntr(temp);

	retval = __sceSasSetKeyOn(&sasCore, voice);
	schedf("__sceSasSetKeyOn: %08x\n", retval);

	bool enablePlayback = true;

	int audioChannel = -1;
	if (enablePlayback) {
		audioChannel = sceAudioChReserve(0, grainSampleBytes/2, PSP_AUDIO_FORMAT_STEREO);
		schedf("reserved audio channel: %d\n", audioChannel);
	}

	int frame = 0;
	for (int i = 0; i < 100; i++) {
		sceCtrlPeekBufferPositive(&pad, 1);
		if (pad.Buttons & PSP_CTRL_START) {
			schedf("Cancelled using the start button");
			break;
		}

		printf("=======\n");

		s16 *dataPtr = samples + (frame % 2) * grainSize * 2;
		memset(dataPtr, 0, grainSize * 2 * sizeof(s16));
		retval = __sceSasCore(&sasCore, dataPtr);
		schedf("__sceSasCore: %08x\n", retval);
		retval = sceAudioOutputBlocking(audioChannel, 0x8000, dataPtr);
		schedf("sceAudioOutputBlocking: %08x\n", retval);

		LogAtracContext(atracID, at3.Data(), NULL, true);

		/*
		printf("\n");
		const uint32_t *ptr = (const uint32_t *)ctx;
		for (int i = 0; i < 0x100; i++) {
			printf("%08x ", ptr[i]);
			if (i % 8 == 7) {
				printf("\n");
			}
		}
		printf("\n");
		*/

		// We don't (yet) want to test Sas internals.
		// LogSasChannel(voice);
		frame++;
	}

	if (enablePlayback) {
		sceAudioChRelease(audioChannel);
	}

	sceAtracReleaseAtracID(atracID);

	return 0;
}
