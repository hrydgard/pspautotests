#include <common.h>

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspatrac3.h>
#include <pspaudio.h>
#include <pspctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <psputility.h>

#include "atrac.h"
#include "shared.h"

// We log too much.
extern unsigned int CHECKPOINT_OUTPUT_DIRECT;
extern unsigned int HAS_DISPLAY;

// NOTE: If you just run the binary plain in psplink (and started usbhostfs from the root), the output text files will be under host0:/
// which effectively is the root of the pspautotests tree.

u32 min(u32 a, u32 b) {
	u32 ret = a > b ? b : a;
	return ret;
}

// Double buffering seems to be enough.
#define DEC_BUFFERS 2

void hexDump16(char *p) {
	unsigned char *ptr = (unsigned char *)p;
	/*
	int i = 0;
	for (; i < 4096; i++) {
		if (ptr[i] != 0) {
			break;
			i++;
		}
	}*/
	// Previously also logged alphabetically but the garbage characters caused git to regard the file as binary.
	schedf("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7], ptr[8], ptr[9], ptr[10], ptr[11], ptr[12], ptr[13], ptr[14], ptr[15]);
}

enum AtracTestMode {
	ATRAC_TEST_FULL = 1,
	ATRAC_TEST_HALFWAY = 2,
	ATRAC_TEST_STREAM = 3,
	ATRAC_TEST_HALFWAY_STREAM = 4,
	ATRAC_TEST_MODE_MASK = 7,

	ATRAC_TEST_CORRUPT = 0x100,
	ATRAC_TEST_DONT_REFILL = 0x200,
	ATRAC_TEST_RESET_POSITION_EARLY = 0x400,
	ATRAC_TEST_RESET_POSITION_LATE = 0x800,
	ATRAC_TEST_RESET_POSITION_RELOAD_ALL = 0x1000,
};

static const char *AtracTestModeToString(AtracTestMode mode) {
	switch (mode & ATRAC_TEST_MODE_MASK) {
	case ATRAC_TEST_FULL: return "full";
	case ATRAC_TEST_HALFWAY: return "halfway";
	case ATRAC_TEST_STREAM: return "stream";
	default: return "N/A";
	}
}

inline void LogResetBuffer(u32 result, int sample, const AtracResetBufferInfo &resetInfo, const u8 *bufPtr) {
	schedf("%08x=sceAtracGetBufferInfoForResetting(%d):\n", result, sample);
	for (int i = 0; i < 2; i++) {
		const AtracSingleResetBufferInfo &info = i == 0 ? resetInfo.first : resetInfo.second;
		schedf("  %s: writeOffset: %08x writableBytes: %08x minWriteBytes: %08x filePos: %08x\n",
			i == 0 ? "first " : "second", (info.writePos == (const u8*)0xcccccccc) ? 0xcccccccc : info.writePos - bufPtr, info.writableBytes, info.minWriteBytes, info.filePos);
	}
}

void LogResetBufferInfo(int atracID, const u8 *bufPtr) {
	static const int sampleOffsets[12] = { -2048, -10000, 0, 1, 2047, 2048, 2049, 4096, 16384, 20000, 40000, 1000000000, };
	for (size_t i = 0; i < ARRAY_SIZE(sampleOffsets); i++) {
		AtracResetBufferInfo resetInfo;
		memset(&resetInfo, 0xcc, sizeof(resetInfo));
		const u32 result = sceAtracGetBufferInfoForResetting(atracID, sampleOffsets[i], &resetInfo);
		LogResetBuffer(result, sampleOffsets[i], resetInfo, (const u8 *)bufPtr);
	}
}

bool RunAtracTest(const char *filename, AtracTestMode mode, int requestedBufSize, int minRemain, int loopCount, bool enablePlayback) {
	schedf("============================================================\n");
	schedf("AtracTest: '%s', mode %s, buffer size %08x, min remain %d:\n", filename, AtracTestModeToString(mode), requestedBufSize, minRemain);

    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	CHECKPOINT_OUTPUT_DIRECT = 1;
	HAS_DISPLAY = 0;  // don't waste time logging to the screen.

	// Pretty small streaming buffer, so even sample.at3 will need to wrap around more than once.
	// Curious size: 0x4100 results in no wrapping of the buffer.
	// const int blk_size = 0x4200;
	int buf_size = 0;

	// doesn't matter, as long as it's bigger than a frame size (0x800 bytes, or the return value of sceAtracGetMaxSample).
	// We double buffer this, otherwise we get playback glitches.
	int decode_size = 32 * 1024;

	FILE *file;

	u32 writePtr;
	u32 readFileOffset;

	// We start by just reading the header.
	// sample_long.at3 streams just as well as sample.at3.
	// However, some offsets become different, which is good for testing. We should make both work.
	int file_size;
	int at3_size;
	int load_bytes;
	u8 *at3_data = 0;
	u8 *decode_data = 0;
	if ((file = fopen(filename, "rb")) != NULL) {
		fseek(file, 0, SEEK_SET);
		u32 header[4];
		fread(&header, 4, 4, file);
		file_size = header[1];
		schedf("filesize (according to header) = 0x%08x\n", file_size);

		hexDump16((char *)header);

		fseek(file, 0, SEEK_END);
		at3_size = ftell(file);
		fseek(file, 0, SEEK_SET);

		schedf("at3size = 0x%08x\n", at3_size);

		switch (mode & ATRAC_TEST_MODE_MASK) {
		case ATRAC_TEST_STREAM:
			buf_size = requestedBufSize;
			load_bytes = requestedBufSize;
			schedf("Streaming. buf_size: %08x\n", buf_size);
			break;
		case ATRAC_TEST_HALFWAY:
			schedf("Creating a buffer that fits the full %08x bytes, but partially filled.\n", at3_size);
			buf_size = at3_size;
			load_bytes = requestedBufSize;
			break;
		case ATRAC_TEST_HALFWAY_STREAM:
			if (requestedBufSize >= at3_size) {
				schedf("UNINTENDED: Creating a buffer that fits the full %08x bytes, but partially filled.\n", at3_size);
				buf_size = at3_size;
				load_bytes = requestedBufSize;
			} else {
				schedf("Creating a streaming buffer that's partially filled.\n");
				// GTA: Vice City Stories does this.
				buf_size = requestedBufSize;
				load_bytes = 0x800;
			}
			break;
		case ATRAC_TEST_FULL:
			schedf("Creating a buffer that fits the full %08x bytes.\n", at3_size);
			buf_size = at3_size;
			load_bytes = at3_size;
			break;
		}

		at3_data = (u8 *)malloc(buf_size);
		decode_data = (u8 *)malloc(decode_size * DEC_BUFFERS);

		memset(at3_data, 0, buf_size);
		memset(decode_data, 0, decode_size);

		fread(at3_data, load_bytes, 1, file);

		if (mode & ATRAC_TEST_CORRUPT) {
			schedf("Corrupting the data.\n");
			int corruptionLocation = load_bytes - 0x200;
			if (corruptionLocation > 0x4000) {
				corruptionLocation = 0x4000;
			}
			memset(at3_data + corruptionLocation, 0xFF, 0x200);
		}
	}

	int id = sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	int id2 = sceUtilityLoadModule(PSP_MODULE_AV_ATRAC3PLUS);

	if ((id >= 0 || (u32) id == 0x80020139UL) && (id2 >= 0 || (u32) id2 == 0x80020139UL)) {
		schedf("Audio modules: OK\n");
	} else {
		schedf("Audio modules: Failed %08x %08x\n", id, id2);
	}

	schedf("Header: %.*s\n", 4, (char *)at3_data);
	schedf("at3_size: %d bufSize: %08x. minRemain: %d.\n", at3_size, buf_size, minRemain);

	int atracID = -1337;
	// set first block of data
	switch (mode & ATRAC_TEST_MODE_MASK) {
	case ATRAC_TEST_STREAM:
	case ATRAC_TEST_FULL:
		atracID = sceAtracSetDataAndGetID(at3_data, load_bytes);
		if (atracID < 0) {
			schedf("sceAtracSetDataAndGetID: Failed %08x\n", atracID);
			return 1;
		} else {
			schedf("sceAtracSetDataAndGetID: OK, size=%08x (file_size: %08x)\n", load_bytes, at3_size);
		}
		break;
	case ATRAC_TEST_HALFWAY:
	case ATRAC_TEST_HALFWAY_STREAM:
		atracID = sceAtracSetHalfwayBufferAndGetID((u8 *)at3_data, load_bytes, buf_size);
		if (atracID < 0) {
			schedf("sceAtracSetHalfwayBufferAndGetID: Failed %08x\n", atracID);
			return 1;
		} else {
			schedf("sceAtracSetHalfwayBufferAndGetID: OK, load=%08x buf=%08x file_size: %08x\n", load_bytes, buf_size, at3_size);
		}
		break;
	default:
		schedf("bad mode\n");
		return 1;
	}

	hexDump16((char *)at3_data);

	int result;
	int endSample, loopStart, loopEnd;
	result = sceAtracGetSoundSample(atracID, &endSample, &loopStart, &loopEnd);
	schedf("%08x=sceAtracGetSoundSample: %08x, %08x, %08x\n", result, endSample, loopStart, loopEnd);

	int bitrate;
	result = sceAtracGetBitrate(atracID, &bitrate);
	schedf("%08x=sceAtracGetBitrate: %d\n", result, bitrate);

	// TODO: Need some mono test data

	u32 channelNum;
	result = sceAtracGetChannel(atracID, &channelNum);
	schedf("%08x=sceAtracGetChannel: %d\n", result, channelNum);

	u32 outputChannel;
	result = sceAtracGetOutputChannel(atracID, &outputChannel);
	schedf("%08x=sceAtracGetOutputChannel: %d\n", result, outputChannel);

	int loopNum = 0xcccccccc;
	u32 loopStatus = 0xcccccccc;
	result = sceAtracGetLoopStatus(atracID, &loopNum, &loopStatus);
	schedf("%08x=sceAtracGetLoopStatus(%d, %08x)\n", result, loopNum, loopStatus);

	result = sceAtracSetLoopNum(atracID, loopCount);
	schedf("%08x=sceAtracSetLoopNum(%d)\n", result, loopCount);

	int maxSamples = 0;
	result = sceAtracGetMaxSample(atracID, &maxSamples);
	schedf("%08x=sceAtracGetMaxSample: %d (%08x)\n", result, maxSamples, maxSamples);

	int nextSamples = 0;
	result = sceAtracGetNextSample(atracID, &nextSamples);
	schedf("%08x=sceAtracGetNextSample: %d (%08x)\n", result, nextSamples, nextSamples);

	int audioChannel = -1;
	if (enablePlayback) {
		audioChannel = sceAudioChReserve(0, maxSamples, PSP_AUDIO_FORMAT_STEREO);
	}

	u32 secondPosition;
	u32 secondDataByte;
	result = sceAtracGetSecondBufferInfo(atracID, &secondPosition, &secondDataByte);
	schedf("%08x=sceAtracGetSecondBufferInfo: %u, %u\n", result, (unsigned int)secondPosition, (unsigned int)secondDataByte);

	int remainFrame = 0xcccccccc;
	result = sceAtracGetRemainFrame(atracID, &remainFrame);
	schedf("sceAtracGetRemainFrame(): %d\n\n", remainFrame);

	// Do an early query just to see what happens here.
	u32 bytesToRead;
	result = sceAtracGetStreamDataInfo(atracID, (u8**)&writePtr, &bytesToRead, &readFileOffset);
	schedf("%i=sceAtracGetStreamDataInfo: %d (offset), %d, %d (%08x %08x %08x)\n", result,
		 (const u8 *)writePtr - at3_data, bytesToRead, readFileOffset,
		 (const u8 *)writePtr - at3_data, bytesToRead, readFileOffset);

	bool first = true;
	if (sizeof(SceAtracIdInfo) != 128) {
		schedf("bad size %d\n", sizeof(SceAtracIdInfo));
	}

	if (mode & ATRAC_TEST_RESET_POSITION_EARLY) {
		schedf("==========================\n");
		schedf("Option enabled: Resetting the buffer position.\n");

		AtracResetBufferInfo resetInfo;
		memset(&resetInfo, 0xcc, sizeof(resetInfo));

		const int seekSamplePos = 0;
		result = sceAtracGetBufferInfoForResetting(atracID, seekSamplePos, &resetInfo);
		LogResetBuffer(result, seekSamplePos, resetInfo, (const u8 *)at3_data);

		int bytesToWrite = resetInfo.first.writableBytes;
		if (!(mode & ATRAC_TEST_RESET_POSITION_RELOAD_ALL)) {
			if (bytesToWrite > requestedBufSize) {
				bytesToWrite = requestedBufSize;
			}
		}

		schedf("Performing actions to reset the buffer - fread of %d/%d bytes from %d in file, to offset %d\n", bytesToWrite, resetInfo.first.writableBytes, resetInfo.first.filePos, resetInfo.first.writePos - at3_data);
		fseek(file, resetInfo.first.filePos, SEEK_SET);
		int writtenBytes1 = fread(resetInfo.first.writePos, 1, bytesToWrite, file);
		int writtenBytes2 = 0;

		LogAtracContext(atracID, (u32)(uintptr_t)at3_data, first);

		result = sceAtracResetPlayPosition(atracID, seekSamplePos, writtenBytes1, writtenBytes2);
		schedf("%08x=sceAtracResetPlayPosition(%d, %d, %d)\n", result, seekSamplePos, writtenBytes1, writtenBytes2);
	}

	int count = 0;
	int end = 3;
	int samples = 0;

	int decIndex = 0;

	bool quit = false;

	while (!(quit && end == 0)) {
		sceCtrlPeekBufferPositive(&pad, 1);
		if (pad.Buttons & PSP_CTRL_START) {
			schedf("Cancelled using the start button");
			break;
		}
		if (quit) {
			// Simple mechanism to try to decode a couple of extra frames at the end, so we
			// can test the errors.
			end--;
		}

		LogAtracContext(atracID, (u32)(uintptr_t)at3_data, first);

		u8 *dec_frame = decode_data + decode_size * decIndex;

		u32 nextDecodePosition = 0;
		result = sceAtracGetNextDecodePosition(atracID, &nextDecodePosition);
		// TODO: We should check sceAtracGetNextSample here, too.
		// result = sceAtracGetNextSample(atracID, &nextDecodePosition);
		schedf("%08x=sceAtracGetNextDecodePosition: %d (%08x)\n", result, nextDecodePosition, nextDecodePosition);
		// decode
		int finish = 0;
		result = sceAtracDecodeData(atracID, (u16 *)(dec_frame), &samples, &finish, &remainFrame);
		if (finish) {
			schedf("Finish flag hit, stopping soon. result=%08x\n", result);
			quit = true;
		} else {
			schedf("(no finish flag) result=%08x\n", result);
		}
		if (result) {
			schedf("%08x=sceAtracDecodeData error: samples: %08x, finish: %08x, remainFrame: %d\n",
				result, samples, finish, remainFrame);
			quit = true;
		} else {
			schedf("%08x=sceAtracDecodeData: samples: %08x, finish: %08x, remainFrame: %d\n",
				result, samples, finish, remainFrame);
		}

		// de-glitch the first frame, which is usually shorter
		if (first && samples < maxSamples) {
			schedf("Deglitching first frame\n");
			memmove(dec_frame + (maxSamples - samples) * 4, dec_frame, samples * 4);
			memset(dec_frame, 0, (maxSamples - samples) * 4);
		}

		if (enablePlayback) {
			// output sound. 0x8000 is the volume, not the block size, that's specified in sceAudioChReserve.
			sceAudioOutputBlocking(audioChannel, 0x8000, dec_frame);
		}

		if (count == 0) {
			// LogResetBufferInfo(atracID, at3_data);
		}

		schedf("========\n");

		result = sceAtracGetStreamDataInfo(atracID, (u8**)&writePtr, &bytesToRead, &readFileOffset);
		schedf("%i=sceAtracGetStreamDataInfo: %d (off), %d, %d (%08x %08x %08x)\n", result,
			(const u8 *)writePtr - at3_data, bytesToRead, readFileOffset,
			(const u8 *)writePtr - at3_data, bytesToRead, readFileOffset);

		// When not needing data, remainFrame is negative.
		if (remainFrame >= 0 && remainFrame < minRemain) {
			// get stream data info
			if (bytesToRead > 0 && (mode & ATRAC_TEST_DONT_REFILL) == 0) {
				int filePos = ftell(file);
				// In halfway buffer mode, restrict the read size, for a more realistic simulation.
				if ((mode & ATRAC_TEST_MODE_MASK) == ATRAC_TEST_HALFWAY) {
					bytesToRead = min(bytesToRead, requestedBufSize);
				}
				if ((int)readFileOffset != filePos) {
					schedf("Calling fread (%d) (!!!! should be at %d, is at %d). Seeking.\n", bytesToRead, readFileOffset, filePos);
					fseek(file, readFileOffset, SEEK_SET);
				} else {
					schedf("Calling fread (%d) (at file offset %d)\n", bytesToRead, readFileOffset);
				}

				int bytesRead = fread((u8*)writePtr, 1, bytesToRead, file);
				if (bytesRead != (int)bytesToRead) {
					schedf("fread error: %d != %d\n", bytesRead, bytesToRead);
					return 1;
				}
				LogAtracContext(atracID, (u32)(uintptr_t)at3_data, first);

				result = sceAtracAddStreamData(atracID, bytesToRead);
				if (result) {
					schedf("%08x=sceAtracAddStreamData(%d) error\n", result, bytesToRead);
					return 1;
				}
				schedf("%08x=sceAtracAddStreamData: %08x\n", result, bytesToRead);

				schedf("========\n");

				// Let's get better information by adding another test here.
				result = sceAtracGetStreamDataInfo(atracID, (u8**)&writePtr, &bytesToRead, &readFileOffset);
				schedf("%i=sceAtracGetStreamDataInfo: %d (off), %d, %d (%08x %08x %08x)\n", result,
					(const u8 *)writePtr - at3_data, bytesToRead, readFileOffset,
					(const u8 *)writePtr - at3_data, bytesToRead, readFileOffset);
			}
		}

		int internalError;
		result = sceAtracGetInternalErrorInfo(atracID, &internalError);
		if (internalError != 0 || result != 0) {
			schedf("%08x=sceAtracGetInternalErrorInfo(): %08x\n", result, internalError);
		}

		first = false;

		decIndex++;
		if (decIndex == DEC_BUFFERS) {
			decIndex = 0;
		}

		if (count % 10 == 0) {
			// This doesn't seem to change with position.
			// LogResetBufferInfo(atracID, (const u8 *)at3_data);
		}

		count++;
	}

	loopNum = 0xcccccccc;
	loopStatus = 0xcccccccc;

	result = sceAtracGetLoopStatus(atracID, &loopNum, &loopStatus);
	schedf("(end) %08x=sceAtracGetLoopStatus(%d, %08x)\n", result, loopNum, loopStatus);

	result = sceAtracGetNextSample(atracID, &nextSamples);
	schedf("(end) %08x=sceAtracGetNextSample: %d (%08x)\n", result, nextSamples, nextSamples);

	LogResetBufferInfo(atracID, (const u8 *)at3_data);
	LogAtracContext(atracID, (u32)(uintptr_t)at3_data, true);

	if (end) {
		schedf("reached end of file\n");
	}

	free(at3_data);
	fclose(file);

	if (enablePlayback) {
		sceAudioChRelease(audioChannel);
	}

	result = sceAtracReleaseAtracID(atracID);
	schedf("sceAtracReleaseAtracID: %08X\n\n", result);

	schedf("Done! req=%d\n", requestedBufSize);
	return true;
}

extern "C" int main(int argc, char *argv[]) {
	// ignore return values for now.
	//RunAtracTest("sample.at3", (AtracTestMode)(ATRAC_TEST_HALFWAY_STREAM | ATRAC_TEST_RESET_POSITION_EARLY), 32 * 1024, 10, 0, false);
	//RunAtracTest("sample.at3", (AtracTestMode)(ATRAC_TEST_HALFWAY_STREAM), 31 * 1024, 10, 0, false);
	RunAtracTest("sample.at3", (AtracTestMode)(ATRAC_TEST_HALFWAY | ATRAC_TEST_RESET_POSITION_EARLY), 0x4000, 10, 0, true);
	//RunAtracTest("sample.at3", (AtracTestMode)(ATRAC_TEST_FULL), 0, 10, 0, false);
	//RunAtracTest("sample.at3", (AtracTestMode)(ATRAC_TEST_STREAM), 0x4000, 10, 0, false);
	// RunAtracTest("sample.at3", (AtracTestMode)(ATRAC_TEST_STREAM | ATRAC_TEST_CORRUPT), 0x3700, 10, 0, false);
	// RunAtracTest("sample.at3", (AtracTestMode)(ATRAC_TEST_STREAM | ATRAC_TEST_DONT_REFILL), 0x4300, 10, 0, false);
	return 0;
}
