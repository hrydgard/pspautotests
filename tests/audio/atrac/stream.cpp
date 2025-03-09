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

// NOTE: If you just run the binary plain in psplink (and started usbhostfs from the root), the output text files will be under host0:/
// which effectively is the root of the pspautotests tree.

u32 min(u32 a, u32 b) {
	u32 ret = a > b ? b : a;
	return ret;
}

// Double buffering seems to be enough.
#define DEC_BUFFERS 2

void logContext(int atracID, u32 buffer, bool full) {
	// Need to call this every time to get the values updated - but only in the emulator!
	// TODO: All Atrac calls should update the raw context, or we should just directly use the fields.
	SceAtracId *ctx = _sceAtracGetContextAddress(atracID);
	if (full) {
		// Also log some stuff from the codec context, just because.
		// Actually, doesn't seem very useful. inBuf is just the current frame being decoded.
		/*
		printf("CODEC inbuf: %p\n", ctx->codec.inBuf);
		printf("CODEC outbuf: %p\n", ctx->codec.outBuf);
		printf("CODEC edramAddr: %08x\n", ctx->codec.edramAddr);
		// printf("CODEC allocMem: %p\n", ctx->codec.allocMem);
		printf("CODEC neededMem: %d\n", ctx->codec.neededMem);
		printf("CODEC err: %d\n", ctx->codec.err);
		*/
		printf("sampleSize: %04x codec: %04x channels: %d\n", ctx->info.sampleSize, ctx->info.codec, ctx->info.numChan);
		printf("endSample: %08x loopStart: %08x loopEnd: %08x\n", ctx->info.endSample, ctx->info.loopStart, ctx->info.loopEnd);
		printf("bufferByte: %08x secondBufferByte: %08x\n", ctx->info.bufferByte, ctx->info.secondBufferByte);
	}

	printf("decodePos: %08x unk22: %08x state: %d numFrame: %02x\n", ctx->info.decodePos, ctx->info.unk22, ctx->info.state, ctx->info.numFrame);
	printf("dataOff: %08x curOff: %08x dataEnd: %08x loopNum: %d\n", ctx->info.dataOff, ctx->info.curOff, ctx->info.dataEnd, ctx->info.loopNum);
	printf("streamDataByte: %08x streamOff: %08x unk52: %08x\n", ctx->info.streamDataByte, ctx->info.streamOff, ctx->info.unk52);
	// Probably shouldn't log these raw pointers (buffer/secondBuffer).
	printf("buffer(offset): %08x secondBuffer: %08x samplesPerChan: %04x\n", ctx->info.buffer - buffer, ctx->info.secondBuffer, ctx->info.samplesPerChan);

	for (int i = 0; i < ARRAY_SIZE(ctx->info.unk); i++) {
		if (ctx->info.unk[i] != 0) {
			printf("unk[%d]: %08x\n", i, ctx->info.unk[i]);
		}
	}
}

void hexDump16(char *p) {
	unsigned char *ptr = (unsigned char *)p;
	// Previously also logged alphabetically but the garbage characters caused git to regard the file as binary.
	printf("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7], ptr[8], ptr[9], ptr[10], ptr[11], ptr[12], ptr[13], ptr[14], ptr[15]);
}

bool RunAtracTest(const char *filename, int requestedBufSize, int minRemain, bool enablePlayback) {
    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	// Pretty small streaming buffer, so even sample.at3 will need to wrap around more than once.
	// Curious size: 0x4100 results in no wrapping of the buffer.
	// const int blk_size = 0x4200;
	int blk_size = 0;

	// doesn't matter, as long as it's bigger than a frame size (0x800 bytes, or the return value of sceAtracGetMaxSample).
	// We double buffer this, otherwise we get playback glitches.
	int decode_size = 32 * 1024;

	FILE *file;

	u32 writePtr;
	u32 readFileOffset;

	// We start by just reading the header.
	// sample_long.at3 streams just as well as sample.at3.
	// However, some offsets become different, which is good for testing. We should make both work.
	char *at3_data;
	int file_size;
	int at3_size;
	char *decode_data;
	if ((file = fopen(filename, "rb")) != NULL) {
		fseek(file, 0, SEEK_SET);
		u32 header[4];
		fread(&header, 4, 4, file);
		file_size = header[1];
		printf("filesize (according to header) = 0x%08x\n", file_size);

		hexDump16((char *)header);

		fseek(file, 0, SEEK_END);
		at3_size = ftell(file);
		fseek(file, 0, SEEK_SET);

		printf("at3size = 0x%08x\n", at3_size);

		if (requestedBufSize > 0) {
			blk_size = requestedBufSize;
			printf("blk_size: %08x\n", blk_size);
		} else if (requestedBufSize == 0) {
			// Load full.
			printf("load full (%08x)\n", at3_size);
			blk_size = at3_size;
		}

		at3_data = (char *)malloc(blk_size);
		decode_data = (char *)malloc(decode_size * DEC_BUFFERS);

		memset(at3_data, 0, blk_size);
		memset(decode_data, 0, decode_size);

		fread(at3_data, blk_size, 1, file);
	}

	int id = sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
	int id2 = sceUtilityLoadModule(PSP_MODULE_AV_ATRAC3PLUS);

	if ((id >= 0 || (u32) id == 0x80020139UL) && (id2 >= 0 || (u32) id2 == 0x80020139UL)) {
		printf("Audio modules: OK\n");
	} else {
		printf("Audio modules: Failed %08x %08x\n", id, id2);
	}

	printf("Header: %.*s\n", 4, (char *)at3_data);
	printf("at3_size: %d bufSize: %08x. minRemain: %d.\n", at3_size, blk_size, minRemain);

	// set first block of data
	int atracID = sceAtracSetDataAndGetID(at3_data, blk_size);
	if (atracID < 0) {
		printf("sceAtracSetDataAndGetID: Failed %08x\n", atracID);
		return 1;
	} else {
		printf("sceAtracSetDataAndGetID: OK, size=%08x (file_size: %08x)\n", blk_size, at3_size);
		at3_size -= blk_size;
	}
	hexDump16((char *)at3_data);

	int result;
	int endSample, loopStart, loopEnd;
	result = sceAtracGetSoundSample(atracID, &endSample, &loopStart, &loopEnd);
	printf("%08x=sceAtracGetSoundSample: %08x, %08x, %08x\n", result, endSample, loopStart, loopEnd);

	int bitrate;
	result = sceAtracGetBitrate(atracID, &bitrate);
	printf("%08x=sceAtracGetBitrate: %d\n", result, bitrate);

	u32 channelNum;
	result = sceAtracGetChannel(atracID, &channelNum);
	printf("%08x=sceAtracGetChannel: %d\n", result, channelNum);

	result = sceAtracSetLoopNum(atracID, 0);
	printf("%08x=sceAtracSetLoopNum\n", result);

	int maxSamples = 0;
	result = sceAtracGetMaxSample(atracID, &maxSamples);
	printf("%08x=sceAtracGetMaxSample: %d (%08x)\n", result, maxSamples, maxSamples);

	int nextSamples = 0;
	result = sceAtracGetNextSample(atracID, &nextSamples);
	printf("%08x=sceAtracGetNextSample: %d (%08x)\n", result, nextSamples, nextSamples);

	int audioChannel = -1;
	if (enablePlayback) {
		audioChannel = sceAudioChReserve(0, maxSamples, PSP_AUDIO_FORMAT_STEREO);
	}

	u32 secondPosition;
	u32 secondDataByte;
	result = sceAtracGetSecondBufferInfo(atracID, &secondPosition, &secondDataByte);
	printf("%08x=sceAtracGetSecondBufferInfo: %u, %u\n", result, (unsigned int)secondPosition, (unsigned int)secondDataByte);

	int end = 0;
	int remainFrame = -1;
	int samples = 0;

	int decIndex = 0;

	result = sceAtracGetRemainFrame(atracID, &remainFrame);
	printf("sceAtracGetRemainFrame(): %d\n\n", remainFrame);

	// Do an early query just to see what happens here.
	u32 bytesToRead;
	result = sceAtracGetStreamDataInfo(atracID, (u8**)&writePtr, &bytesToRead, &readFileOffset);
	printf("%i=sceAtracGetStreamDataInfo: %d (offset), %d, %d (%08x %08x %08x)\n", result, (char *)writePtr - at3_data, bytesToRead, readFileOffset, (char *)writePtr - at3_data, bytesToRead, readFileOffset);

	bool first = true;
	if (sizeof(SceAtracIdInfo) != 128) {
		printf("bad size %d\n", sizeof(SceAtracIdInfo));
	}

	while (!end) {
        sceCtrlPeekBufferPositive(&pad, 1);
		if (pad.Buttons & PSP_CTRL_START) {
			printf("Cancelled using the start button");
			break;
		}

		logContext(atracID, (u32)(uintptr_t)at3_data, first);

		char *dec_frame = decode_data + decode_size * decIndex;

		u32 nextDecodePosition = 0;
		result = sceAtracGetNextDecodePosition(atracID, &nextDecodePosition);
		// TODO: We should check sceAtracGetNextSample here, too.
		// result = sceAtracGetNextSample(atracID, &nextDecodePosition);
		printf("%08x=sceAtracGetNextDecodePosition: %d (%08x)\n", result, nextDecodePosition, nextDecodePosition);
		// decode
		result = sceAtracDecodeData(atracID, (u16 *)(dec_frame), &samples, &end, &remainFrame);
		if (result) {
			printf("%i=sceAtracDecodeData error: samples: %08x, end: %08x, remainFrame: %d\n",
				result, samples, end, remainFrame);
			return false;
		}
		printf("%i=sceAtracDecodeData: samples: %08x, end: %08x, remainFrame: %d\n",
			result, samples, end, remainFrame);

		// de-glitch the first frame, which is usually shorter
		if (first && samples < maxSamples) {
			printf("Deglitching first frame\n");
			memmove(dec_frame + (maxSamples - samples) * 4, dec_frame, samples * 4);
			memset(dec_frame, 0, (maxSamples - samples) * 4);
		}

		if (enablePlayback) {
			// output sound. 0x8000 is the volume, not the block size, that's specified in sceAudioChReserve.
			sceAudioOutputBlocking(audioChannel, 0x8000, dec_frame);
		}

		printf("========\n");

		result = sceAtracGetStreamDataInfo(atracID, (u8**)&writePtr, &bytesToRead, &readFileOffset);
		printf("%i=sceAtracGetStreamDataInfo: %d (off), %d, %d (%08x %08x %08x)\n", result, (char *)writePtr - at3_data, bytesToRead, readFileOffset, (char *)writePtr - at3_data, bytesToRead, readFileOffset);

		if (remainFrame < minRemain) {
			// get stream data info
			if (bytesToRead > 0) {
				int filePos = ftell(file);
				if (readFileOffset != filePos) {
					printf("Calling fread (%d) (!!!! should be at %d, is at %d). Seeking.\n", bytesToRead, readFileOffset, filePos);
					fseek(file, readFileOffset, SEEK_SET);
				} else {
					printf("Calling fread (%d) (at file offset %d)\n", bytesToRead, readFileOffset);
				}
				int bytesRead = fread((u8*)writePtr, 1, bytesToRead, file);
				if (bytesRead != bytesToRead) {
					printf("fread error: %d != %d\n", bytesRead, bytesToRead);
					return 1;
				}
				logContext(atracID, (u32)(uintptr_t)at3_data, first);

				result = sceAtracAddStreamData(atracID, bytesToRead);
				if (result) {
					printf("%08x=sceAtracAddStreamData(%d) error\n", result, bytesToRead);
					return 1;
				}
				printf("%08x=sceAtracAddStreamData: %08x\n", result, bytesToRead);

				printf("========\n");

				// Let's get better information by adding another test here.
				result = sceAtracGetStreamDataInfo(atracID, (u8**)&writePtr, &bytesToRead, &readFileOffset);
				printf("%i=sceAtracGetStreamDataInfo: %d (off), %d, %d (%08x %08x %08x)\n", result, (char *)writePtr - at3_data, bytesToRead, readFileOffset, (char *)writePtr - at3_data, bytesToRead, readFileOffset);
			}
		}

		first = false;

		decIndex++;
		if (decIndex == DEC_BUFFERS) {
			decIndex = 0;
		}
	}

	logContext(atracID, (u32)(uintptr_t)at3_data, true);

	if (end) {
		printf("reached end of file\n");
	}

	free(at3_data);
	fclose(file);

	if (enablePlayback) {
		sceAudioChRelease(audioChannel);
	}

	result = sceAtracReleaseAtracID(atracID);
	printf("sceAtracReleaseAtracID: %08X\n\n", result);

	printf("Done! req=%d\n", requestedBufSize);

	// Sleep for 1 second.
	// sceKernelDelayThread(1000000);
	return true;
}

extern "C" int main(int argc, char *argv[]) {
	// ignore return values for now.
	RunAtracTest("sample.at3", 0x4000, 10, true);
	return 0;
}
