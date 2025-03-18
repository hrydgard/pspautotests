#pragma once

#include <common.h>
#include <string>

#include "atrac.h"

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
	schedf("\n");
	if (info.second.filePos) {
		schedf("   #2: ");
		schedfSingleResetBuffer(info.second, basePtr);
		schedf("\n");
	}
}

inline void forceAtracState(int atracID, int state) {
	SceAtracId *ctx = _sceAtracGetContextAddress(atracID);
	if (ctx) {
		ctx->info.state = state;
	}
}

void LoadAtrac();
void UnloadAtrac();

void LogAtracContext(int atracID, const u8 *buffer, const u8 *secondBuffer, bool full);
void LogResetBuffer(u32 result, int sample, const AtracResetBufferInfo &resetInfo, const u8 *bufPtr);
void LogResetBufferInfo(int atracID, const u8 *bufPtr);
const char *AtracTestModeToString(AtracTestMode mode);

inline void hexDump16(char *p) {
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

struct Atrac3File {
	explicit Atrac3File(const char *filename);
	explicit Atrac3File(size_t size) {
		data_ = new u8[size];
		size_ = size;
		pos_ = 0;
	}
	Atrac3File() {
		data_ = NULL;
		size_ = 0;
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
	int size_;

	// Also add simple file access emulation, for ease of porting tests.
	int pos_;
};

void CreateLoopedAtracFrom(Atrac3File &at3, Atrac3File &updated, int loopStart, int loopEnd, int numLoops = 1);
