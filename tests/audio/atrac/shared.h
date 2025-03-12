#pragma once

#include <common.h>
#include <string>

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
		pos_ = 0;
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

	void Reset() {
		pos_ = 0;
	}

	void Seek(int value, int method) {
		switch (method) {
		case SEEK_SET:
			pos_ = value;
			break;
		case SEEK_CUR:
			pos_ += value;
			break;
		case SEEK_END:
			pos_ = size_ + value;
			break;
		}
	}

	int Read(void *buffer, int readSize) {
		if (pos_ + readSize > size_) {
			printf("Read past the end! pos=%d readSize=%d size_=%d\n", pos_, readSize, size_);
			readSize = size_ - pos_;
		}
		memcpy(buffer, data_ + pos_, readSize);
		pos_ += readSize;
		return readSize;
	}

	int Tell() const {
		return pos_;
	}

private:
	const char *name_;
	u8 *data_;
	size_t size_;

	// Also add simple file access emulation, for ease of porting tests.
	int pos_;
};

void CreateLoopedAtracFrom(Atrac3File &at3, Atrac3File &updated, u32 loopStart, u32 loopEnd);
