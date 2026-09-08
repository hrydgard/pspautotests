#include <common.h>
#include <pspiofilemgr.h>
#include <pspmp3.h>
#include <psputility.h>
#include <pspctrl.h>
#include <psputils.h>
#include <pspaudio.h>

#include "shared.h"

// NOTE: When executed on the PSP, audio playback is choppy. Should probably not
// interleave all of I/O, decoding and playback on the same thread, but it's convenient
// for testing. Don't use this as an example of what to do in a real project.

// We log too much.
extern unsigned int CHECKPOINT_OUTPUT_DIRECT;
extern unsigned int HAS_DISPLAY;

static u8 mp3Buf[8192] __attribute__((aligned(64)));
static short pcmBuf[4608] __attribute__((aligned(64)));

// Functions the SDK doesn't declare.
extern "C" {
SceInt32 sceMp3GetMPEGVersion(SceInt32 handle);
SceInt32 sceMp3GetFrameNum(SceInt32 handle);
}

void LogMp3Context(int handle) {
    u32 loopNum = sceMp3GetLoopNum(handle);
    u32 sumDecSamples = sceMp3GetSumDecodedSample(handle);
    schedf("loop: %08x sum: %08x\n", loopNum, sumDecSamples);
}

static void runtest(const char *filename, bool enablePlayback) {
    schedf("**** MP3Test: %s\n", filename);

    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    int fd = sceIoOpen(filename, PSP_O_RDONLY, 0777);
    if (fd < 0) {
        printf("Failed to open %s\n", filename);
        return;
    }

	SceMp3InitArg mp3Init;
	memset(&mp3Init, 0, sizeof(mp3Init));
	mp3Init.mp3StreamStart = 0;
	mp3Init.mp3StreamEnd = sceIoLseek32(fd, 0, SEEK_END);
	mp3Init.mp3Buf = (SceUChar8 *)(mp3Buf);
	mp3Init.mp3BufSize = sizeof(mp3Buf);
	mp3Init.pcmBuf = (SceUChar8 *)(pcmBuf);
	mp3Init.pcmBufSize = sizeof(pcmBuf);

    schedf("%s: mp3StreamEnd = %08x\n", filename, mp3Init.mp3StreamEnd);

    sceIoLseek32(fd, 0, SEEK_SET);

	int handle = sceMp3ReserveMp3Handle(&mp3Init);

    // The first read happens before sceMp3Init, so we can figure out the format.
	u8 *dst = NULL;
	SceInt32 towrite = 0;
	SceInt32 srcpos = 0;
    int retval = sceMp3GetInfoToAddStreamData(handle, &dst, &towrite, &srcpos);
    schedf("%d=sceMp3GetInfoToAddStreamData: %08x %08x %08x\n", retval, (int)dst, towrite, srcpos);
	sceIoLseek32(fd, srcpos, SEEK_SET);
    sceIoRead(fd, dst, towrite);
    retval = sceMp3NotifyAddStreamData(handle, towrite);
    schedf("%d=sceMp3NotifyAddStreamData: %08x\n", retval, towrite);

    int maxSamples = sceMp3GetMaxOutputSample(handle);
    schedf("%08x=sceMp3GetMaxOutputSample()  (preinit)\n", maxSamples);

    retval = sceMp3Init(handle);
    schedf("%d=sceMp3Init: %08x\n", retval, handle);
    if (retval < 0) {
        sceIoClose(fd);
        return;
    }

    schedf("Version: %08x", sceMp3GetMPEGVersion(handle));

    maxSamples = sceMp3GetMaxOutputSample(handle);
    schedf("%08x/%d=sceMp3GetMaxOutputSample()  (pcmbuf=4608)\n", maxSamples, maxSamples);

    // Let's fetch and print some information.
    int loopNum = sceMp3GetLoopNum(handle);
    schedf("%d=sceMp3GetLoopNum: %08x\n", retval, loopNum);

    u32 frameNum = sceMp3GetFrameNum(handle);
    schedf("%d=sceMp3GetFrameNum: %08x\n", retval, frameNum);

	int audioChannel = -1;
	if (enablePlayback) {
		audioChannel = sceAudioChReserve(0, maxSamples, PSP_AUDIO_FORMAT_STEREO);
	}

    sceMp3SetLoopNum(handle, 0);
    schedf("%d=sceMp3SetLoopNum(%d)\n", retval, 0);

    // OK, we can now enter a stream/decode loop.
    while (true) {
        sceCtrlPeekBufferPositive(&pad, 1);
		if (pad.Buttons & PSP_CTRL_START) {
			schedf("Cancelled using the start button");
			break;
		}

        short *curPcmBuf = NULL;
        int outBytes = sceMp3Decode(handle, &curPcmBuf);
        schedf("%d=sceMp3Decode(offset=%d)\n", outBytes, curPcmBuf - pcmBuf);
        if (outBytes == 0) {
            schedf("sceMp3Decode returned 0, should be the end. Breaking.\n");
            break;
        }
        if (outBytes < 0) {
            schedf("sceMp3Decode returned %08x, bad. Breaking.\n");
            break;
        }

        LogMp3Context(handle);

        // We play the contents here.
		sceAudioOutputBlocking(audioChannel, 0x8000, curPcmBuf);
        // Now, continue streaming.
        if (sceMp3CheckStreamDataNeeded(handle)) {
            schedf("Need more data (loopnum=%d)\n", sceMp3GetLoopNum(handle));
            retval = sceMp3GetInfoToAddStreamData(handle, &dst, &towrite, &srcpos);
            schedf("%d=sceMp3GetInfoToAddStreamData: %08x %08x %08x\n", retval, (int)dst, towrite, srcpos);
            if (retval < 0) {
                break;
            }
            sceIoLseek32(fd, srcpos, SEEK_SET);
            sceIoRead(fd, dst, towrite);
            retval = sceMp3NotifyAddStreamData(handle, towrite);
        }
    }

	if (enablePlayback) {
		sceAudioChRelease(audioChannel);
	}

    sceIoClose(fd);

    sceMp3ReleaseMp3Handle(handle);
}

extern "C" int main(int argc, char *argv[]) {
    CHECKPOINT_OUTPUT_DIRECT = 1;
	HAS_DISPLAY = 0;  // don't waste time logging to the screen.

	sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	sceUtilityLoadModule(PSP_MODULE_AV_MP3);

	sceMp3InitResource();

    runtest("sample.mp3", true);

	sceMp3TermResource();
    return 0;
}
