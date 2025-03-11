#pragma once

#include <common.h>

#include "atrac.h"

inline void schedfSingleResetBuffer(AtracSingleResetBufferInfo &info, void *basePtr) {
	int diff = info.writePos - (u8 *)basePtr;
	if (diff < 0x10000 && diff >= 0) {
		schedf("write=p+0x%x, writable=%08x, min=%08x, file=%08x", diff, info.writableBytes, info.minWriteBytes, info.filePos);
	} else {
		schedf("write=%p, writable=%08x, min=%08x, file=%08x", info.writePos, info.writableBytes, info.minWriteBytes, info.filePos);
	}
}

inline void schedfResetBuffer(AtracResetBufferInfo &info, void *basePtr) {
	schedf("   #1: ");
	schedfSingleResetBuffer(info.first, basePtr);
	schedf("\n   #2: ");
	schedfSingleResetBuffer(info.second, basePtr);
	schedf("\n");
}

inline void forceAtracState(int atracID, int state) {
	SceAtracId *ctx = _sceAtracGetContextAddress(atracID);
	if (ctx) {
		ctx->info.state = state;
	}
}

void LoadAtrac();
void UnloadAtrac();

void LogAtracContext(int atracID, u32 buffer, bool full);

struct Atrac3File {
	Atrac3File(const char *filename);
	Atrac3File(size_t size) {
		data_ = new u8[size];
		size_ = size;
	}
	~Atrac3File();

	void Reload(const char *filename);
	void Require();

	void Reset(size_t size) {
		delete [] data_;
		data_ = new u8[size];
		size_ = size;
	}

	bool IsValid() {
		return data_ != NULL;
	}
	u8 *Data() {
		return data_;
	}
	size_t Size() {
		return size_;
	}

private:
	size_t size_;
	u8 *data_;
};

void CreateLoopedAtracFrom(Atrac3File &at3, Atrac3File &updated, u32 loopStart, u32 loopEnd);
