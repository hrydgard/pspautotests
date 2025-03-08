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

#define DEC_BUFFERS 3

extern "C" int main(int argc, char *argv[]) {
    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	char *at3_data;
	int file_size;
	int at3_size;

	const int blk_size = 0x8000;
	const int minRemain = 20;

	char *decode_data;

	// doesn't matter, as long as it's bigger than a frame size (0x800 bytes, or the return value of sceAtracGetMaxSample).
	int decode_size = 32 * 1024;

	FILE *file;

	int atracID;
	int maxSamples = 0;
	int result;

	u32 puiPosition;
	u32 puiDataByte;

	u32 writePtr;
	u32 readOffset;

	// We start by just reading the header.
	// sample_long.at3 streams just as well as sample.at3.
	// However, some offsets become different, which is good for testing. We should make both work.
	if ((file = fopen("sample.at3", "rb")) != NULL) {
		fseek(file, 0, SEEK_SET);
		u32 header[2];
		fread(&header, 4, 2, file);
		file_size = header[1];
		printf("filesize (according to header) = 0x%08x\n", file_size);

		fseek(file, 0, SEEK_END);
		at3_size = ftell(file);
		fseek(file, 0, SEEK_SET);

		printf("at3size = 0x%08x\n", at3_size);

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
	atracID = sceAtracSetDataAndGetID(at3_data, blk_size);
	if (atracID < 0) {
		printf("sceAtracSetDataAndGetID: Failed %08x\n", atracID);
		return 1;
	} else {
		printf("sceAtracSetDataAndGetID: OK, size=%08x (file_size: %08x)\n", blk_size, at3_size);
		at3_size -= blk_size;
	}

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

	result = sceAtracGetMaxSample(atracID, &maxSamples);
	printf("%08x=sceAtracGetMaxSample: %d\n", result, maxSamples);

	int audioChannel = sceAudioChReserve(0, maxSamples, PSP_AUDIO_FORMAT_STEREO);
	printf("sceAudioChReserve: %08X\n", audioChannel);

	result = sceAtracGetSecondBufferInfo(atracID, &puiPosition, &puiDataByte);
	printf("%08x=sceAtracGetSecondBufferInfo: %u, %u\n", (unsigned int)puiPosition, (unsigned int)puiDataByte);

	// Do an early query just to see what happens here.
	u32 temp;
	result = sceAtracGetStreamDataInfo(atracID, (u8**)&writePtr, &temp, &readOffset);
	printf("%i=sceAtracGetStreamDataInfo: %d (off), %d, %d\n", result, (char *)writePtr - at3_data, temp, readOffset);

	SceAtracId *ctx = _sceAtracGetContextAddress(atracID);
	printf("Context: decodePos: %08x, endSample: %08x\n", ctx->info.decodePos, ctx->info.endSample);
	printf("loopStart: %08x, loopEnd: %08x, samplesPerChan: %08x\n", ctx->info.loopStart, ctx->info.loopEnd, ctx->info.samplesPerChan);
	printf("numFrames: %02x, state: %02x, unk22: %02x\n", ctx->info.numFrame, ctx->info.state, ctx->info.unk22);
	printf("numChan: %02x, sampleSize: %04x, codec: %04x\n", ctx->info.numChan, ctx->info.sampleSize, ctx->info.codec);
	printf("dataOff: %08x, curOff: %08x, dataEnd: %08x"\n, ctx->info.dataOff, ctx->info.curOff, ctx->info.dataEnd);
	printf("loopNum: %d, streamDataByte: %08x, unk48: %08x, unk52: %08x\n", ctx->info.loopNum, ctx->info.streamDataByte, ctx->info.unk48, ctx->info.unk52);
	printf("buffer: %d, secondBuffer: %d, bufferByte: %08x, secondBufferByte: %08x\n", ctx->info.buffer != 0, ctx->info.secondBuffer != 0, ctx->info.bufferByte, ctx->info.secondBufferByte);
	printf("\n");

	schedfAtrac(atracID);

	int end = 0;
	int remainFrame = -1;
	int samples = 0;

	int decIndex = 0;

	result = sceAtracGetRemainFrame(atracID, &remainFrame);
	printf("sceAtracGetRemainFrame(): %d\n\n", remainFrame);

	while (!end) {
        sceCtrlPeekBufferPositive(&pad, 1);
		if (pad.Buttons & PSP_CTRL_START) {
			printf("Cancelled using the start button");
			break;
		}

		// Need to call this again to get the values updated - but only in the emulator!
		// TODO: All Atrac calls should update the raw context, or we should just directly use the fields.
		SceAtracId *ctx = _sceAtracGetContextAddress(atracID);

		// Log out some important fields so we can check them.
		printf("ATRAC: decodePos: %08x, curPos: %08x, loopStart: %08x, loopEnd: %08x, samplesPerChan: %08x\n",
			ctx->info.decodePos, ctx->info.endSample, ctx->info.loopStart, ctx->info.loopEnd, ctx->info.samplesPerChan);

		char *dec_frame = decode_data + decode_size * decIndex;

		// decode
		result = sceAtracDecodeData(atracID, (u16 *)(dec_frame), &samples, &end, &remainFrame);
		if (result) {
			printf("%i=sceAtracDecodeData error: samples: %08x, end: %08x, remainFrame: %d\n",
				result, samples, end, remainFrame);
			return -1;
		}
		printf("%i=sceAtracDecodeData: samples: %08x, end: %08x, remainFrame: %d\n",
			result, samples, end, remainFrame);

		// output sound. 0x8000 is the volume, not the block size, that's specified in sceAudioChReserve.
		sceAudioOutputBlocking(audioChannel, 0x8000, dec_frame);

		// Here 170 is a guess frame threshold (too big for the small AT3!)
		// 42 is not unusual (seen in Wipeout Pulse), though higher values are often seen too.
		// Probably depends on the buffer size.
		if (remainFrame < minRemain) {
			// get stream data info
			u32 bytesToRead;
			result = sceAtracGetStreamDataInfo(atracID, (u8**)&writePtr, &bytesToRead, &readOffset);
			printf("%08x=sceAtracGetStreamDataInfo: %d (off), %d, %d\n", result, (char *)writePtr - at3_data, bytesToRead, readOffset);
			if (bytesToRead > 0) {
				int bytesRead = fread((u8*)writePtr, 1, bytesToRead, file);
				if (bytesRead != bytesToRead) {
					printf("fread error: %d != %d\n", bytesRead, bytesToRead);
					return 1;
				}
				result = sceAtracAddStreamData(atracID, bytesToRead);
				if (result) {
					printf("%08x=sceAtracAddStreamData(%d) error\n", result, bytesToRead);
					return 1;
				}
				printf("%08x=sceAtracAddStreamData: %08x\n", result, bytesToRead);
			}
		}

		decIndex++;
		if (decIndex == DEC_BUFFERS) {
			decIndex = 0;
		}
	}

	if (end) {
		printf("reached end of file\n");
	}

	free(at3_data);
	fclose(file);

	result = sceAudioChRelease(audioChannel);
	printf("sceAudioChRelease: %08X\n", result);
	result = sceAtracReleaseAtracID(atracID);
	printf("sceAtracReleaseAtracID: %08X\n\n", result);

	printf("Done!\n");

	// Sleep for 2 seconds.
	sceKernelDelayThread(2000000);

	return 0;
}
