#include <pspatrac3.h>

extern "C" {
	int sceAtracGetSecondBufferInfo(int atracID, u32 *puiPosition, u32 *puiDataByte);
	int sceAtracGetNextDecodePosition(int atracID, u32 *puiSamplePosition);
	int sceAtracGetAtracID(int codecType);
	int sceAtracReinit(int at3origCount, int at3plusCount);
	int sceAtracSetData(int atracID, void *buf, SceSize bufSize);
	int sceAtracResetPlayPosition(int atracID, int sampleCount, int bytesWrittenFirstBuf, int bytesWrittenSecondBuf);
}
