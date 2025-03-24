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

void LogAtracContextForSas(SasCore *sasCore, int atracID, const u8 *buffer, bool full) {
	// Need to call this every time to get the values updated - but only in the emulator!
	// TODO: All Atrac calls should update the raw context, or we should just directly use the fields.
	int bufferOff = (intptr_t)buffer;
	SceAtracId *ctx = _sceAtracGetContextAddress(atracID);
	if (!ctx) {
		schedf("Context not yet available for atracID %d\n", atracID);
		return;
	}
	if (full) {
		int bufOff = (ctx->info.buffer == 0) ? 0 : (intptr_t)ctx->info.buffer - bufferOff;
		// Also log some stuff from the codec context, just because.
		// Actually, doesn't seem very useful. inBuf is just the current frame being decoded.
		// printf("sceAudioCodec inbuf: %p outbuf: %p inBytes: %d outBytes: %d\n", ctx->codec.inBuf, ctx->codec.outBuf, ctx->codec.inBytes, ctx->codec.outBytes);
		schedf("dataOff: %08x sampleSize: %04x codec: %04x channels: %d\n", ctx->info.dataOff, ctx->info.sampleSize, ctx->info.codec, ctx->info.numChan);
		schedf("endSample: %08x loopStart: %08x loopEnd: %08x\n", ctx->info.endSample, ctx->info.loopStart, ctx->info.loopEnd);
		schedf("bufferByte: %08x secondBufferByte: %08x\n", ctx->info.bufferByte, ctx->info.secondBufferByte);
		schedf("buffer(offset): %08x\n", bufOff);
	}

	schedf("curFileOff: %08x fileDataEnd: %08x decodePos: %08x endFlag: %08x\n", ctx->info.curFileOff, ctx->info.fileDataEnd, ctx->info.decodePos, __sceSasGetEndFlag(sasCore));
	schedf("second: %08x secondbyte: %08x buffer: %08x err: %08x ctx: %08x\n", ctx->info.secondBuffer, ctx->info.secondBufferByte, (u32)buffer, ctx->codec.err, (u32)ctx);

	// schedf("buffer %08x secondBuffer %08x firstValidSample: %04x\n", ctx->info.buffer, ctx->info.secondBuffer, ctx->info.firstValidSample);

	for (int i = 0; i < ARRAY_SIZE(ctx->info.unk); i++) {
		if (ctx->info.unk[i] != 0) {
			schedf("unk[%d]: %08x\n", i, ctx->info.unk[i]);
		}
	}
}

// Supported modes are FULL and STREAM.
// The initialStreamSize and streamBlockSize are only used for STREAM mode.
int SasAtracTest(Atrac3File &at3, AtracTestMode mode, int initialStreamSize, int streamBlockSize) {
    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	u8 *readBuf[2] = {
		(u8 *)malloc(streamBlockSize),
		(u8 *)malloc(streamBlockSize),
	};

	if (initialStreamSize >= at3.Size()) {
		printf("Pointless to stream, buffer too big\n");
		return 1;
	}

	int atracID = -1;
	switch (mode) {
	case ATRAC_TEST_FULL:
		atracID = sceAtracSetDataAndGetID((void*)at3.Data(), at3.Size());
		break;
	case ATRAC_TEST_STREAM:
		atracID = sceAtracSetDataAndGetID(at3.Data(), initialStreamSize);
		// Let's start the stream at the correct location.
		at3.Seek(initialStreamSize, SEEK_SET);
		break;
	default:
		schedf("Unsupported test mode %d\n", mode);
		return 1;
	}

	if (atracID < 0) {
		schedf("sceAtracSetDataAndGetID: Failed %08x\n", atracID);
		return 1;
	} else {
		schedf("sceAtracSetDataAndGetID(..., %d): OK\n", at3.Size());
	}
	LogAtracContext(atracID, at3.Data(), NULL, true);

	const int grainSize = 1024;
	const int grainSampleBytes = grainSize * 4;

	SceAtracId *ctx = (SceAtracId *)_sceAtracGetContextAddress(atracID);
	schedf("_sceAtracGetContextAddress: ...   streamOff=%08x\n", ctx->info.streamOff);

	int retval = __sceSasInit(&sasCore, grainSize, 32, 1, 44100);
	schedf("sceSasInit: %08x\n", retval);

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

	int oldState = ctx->info.state;
	printf("Old state: %d\n", oldState);

	ctx->info.numChan = 1;   // Only mono supported.
	ctx->info.loopEnd = 0;
	ctx->info.state = 0x10;  // Set the secret Atrac3 state.
	ctx->codec.err = 0;
	retval = __sceSasSetVoiceATRAC3(&sasCore, voice, ctx);
	schedf("__sceSasSetVoiceATRAC3: %08x\n", retval);
	if ((int)retval < 0) {
		// Bad.
		return 1;
	}
	LogAtracContextForSas(&sasCore, atracID, at3.Data(), true);

    int temp = sceKernelCpuSuspendIntr();

	// This setup is shared whether we stream or not.

	ctx->info.buffer += ctx->info.streamOff;
	ctx->info.loopNum = voice;
	ctx->info.curFileOff += ctx->info.streamDataByte;

	if (oldState != 2) {
		// If we aren't in FULL mode, mark for streaming.
		ctx->info.secondBuffer = (u8 *)0xFFFFFFFF;
	}

	sceKernelCpuResumeIntr(temp);

	schedf("Modified the context.\n");

	LogAtracContextForSas(&sasCore, atracID, at3.Data(), false);
	// LogSasChannel(voice);

	retval = __sceSasSetADSR(&sasCore,voice,0xf,0x40000000,0,0,0x10000);
	schedf("__sceSasSetADSR: %08x\n", retval);

	retval = __sceSasSetPitch(&sasCore, voice, 4096);

	retval = __sceSasSetKeyOn(&sasCore, voice);
	schedf("__sceSasSetKeyOn: %08x\n", retval);

	bool enablePlayback = true;

	int audioChannel = -1;
	if (enablePlayback) {
		audioChannel = sceAudioChReserve(0, grainSampleBytes/2, PSP_AUDIO_FORMAT_STEREO);
		schedf("reserved audio channel: %d\n", audioChannel);
	}

	int frame = 0;
	while (true) {
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

		LogAtracContextForSas(&sasCore, atracID, at3.Data(), false);

		/*
		printf("\n");
		const uint32_t *ptr = (const uint32_t *)ctx;
		for (int i = 0; i < 0x20; i++) {
			printf("%08x ", (unsigned int)ptr[i]);
			if (i % 8 == 7) {
				printf("\n");
			}
		}
		printf("\n");
		*/

		if (mode == ATRAC_TEST_STREAM) {
			int needMore = 0;
			// Check with the context whether more data is needed.
			if (ctx->info.curFileOff == ctx->info.fileDataEnd) {
				if (ctx->info.secondBuffer != 0) {
					needMore = (int)ctx->info.secondBuffer < 1;
				}
			} else {
				needMore = (int)ctx->info.secondBuffer < 1;
			}

			if (needMore) {
				schedf("!!!!!!!!!!!!!!!! Need more data! reading %08x bytes at %08x\n", streamBlockSize, at3.Tell());

				static int curBuf = 0;
				curBuf ^= 1;

				int sizeToAdd = at3.Read(readBuf[curBuf], streamBlockSize);

				const u8 *readPtr = readBuf[curBuf];

				int remaining = ctx->info.fileDataEnd - ctx->info.curFileOff;
				if (remaining < sizeToAdd) {
					sizeToAdd = remaining;
				}
				if (sizeToAdd == 0) {
					readPtr = NULL;
				}
				if (sizeToAdd == remaining || sizeToAdd > 0x3ff) {
					ctx->info.curFileOff += sizeToAdd;
					retval = __sceSasConcatenateATRAC3(&sasCore, voice, readPtr, sizeToAdd);
					schedf("%08x=__sceSasConcatenateATRAC3(%d, %p, %d)\n", retval, voice, readPtr, sizeToAdd);
					LogAtracContextForSas(&sasCore, atracID, at3.Data(), false);
				}
			}
		}

		// We don't (yet) want to test Sas internals.
		// LogSasChannel(voice);
		frame++;

		if (__sceSasGetEndFlag(&sasCore) == 0xFFFFFFFF) {
			schedf("All notes are done\n");
			break;
		}
	}

	if (enablePlayback) {
		sceAudioChRelease(audioChannel);
	}

	sceAtracReleaseAtracID(atracID);
	free(readBuf[0]);
	free(readBuf[1]);
	return 0;
}

extern "C" int main(int argc, char *argv[]) {
	CHECKPOINT_OUTPUT_DIRECT = 1;
	HAS_DISPLAY = 0;

	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_SASCORE);
	sceUtilityLoadModule(PSP_MODULE_AV_ATRAC3PLUS);

	Atrac3File at3("test_mono_long.at3");
	if (!at3.IsValid()) {
		schedf("Failed to load test_mono_long.at3\n");
		return 1;
	}

	int retval = SasAtracTest(at3, ATRAC_TEST_STREAM, 0x2000, 0x2000);
	if (retval == 0) {
		schedf("Test failed\n");
	}

	// at3.Reload("test_mono.at3");

	schedf("end.\n");

	return retval;
}
