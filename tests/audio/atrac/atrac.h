#include <pspatrac3.h>

extern "C" {
	typedef struct {
		u8 *writePos;
		u32 writableBytes;
		u32 minWriteBytes;
		u32 filePos;
	} AtracSingleResetBufferInfo;

	typedef struct {
		AtracSingleResetBufferInfo first;
		AtracSingleResetBufferInfo second;
	} AtracResetBufferInfo;

	int sceAtracGetSecondBufferInfo(int atracID, u32 *puiPosition, u32 *puiDataByte);
	int sceAtracGetNextDecodePosition(int atracID, u32 *puiSamplePosition);
	int sceAtracGetAtracID(uint codecType);
	int sceAtracReinit(int at3origCount, int at3plusCount);
	int sceAtracSetData(int atracID, u8 *buf, u32 bufSize);
	int sceAtracResetPlayPosition(int atracID, u32 sampleCount, u32 bytesWrittenFirstBuf, u32 bytesWrittenSecondBuf);
	int sceAtracGetBufferInfoForResetting(int atracID, int position, AtracResetBufferInfo *bufferInfo);
	int sceAtracSetHalfwayBufferAndGetID(u8 *buf, u32 bufSize, u32 readBytes);
}
