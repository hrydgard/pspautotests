#pragma once

#include <stdint.h>
#include <vector>

#pragma pack(push, 1)

struct CommandType {
	enum Value {
		INIT = 0,
		REGISTERS = 1,
		VERTICES = 2,
		INDICES = 3,
		CLUT = 4,
		TRANSFERSRC = 5,
		MEMSET = 6,
		MEMCPYDEST = 7,
		MEMCPYDATA = 8,
		DISPLAY = 9,
		CLUTADDR = 10,
		EDRAMTRANS = 11,

		TEXTURE0 = 0x10,
		TEXTURE1 = 0x11,
		TEXTURE2 = 0x12,
		TEXTURE3 = 0x13,
		TEXTURE4 = 0x14,
		TEXTURE5 = 0x15,
		TEXTURE6 = 0x16,
		TEXTURE7 = 0x17,

		FRAMEBUF0 = 0x18,
		FRAMEBUF1 = 0x19,
		FRAMEBUF2 = 0x1A,
		FRAMEBUF3 = 0x1B,
		FRAMEBUF4 = 0x1C,
		FRAMEBUF5 = 0x1D,
		FRAMEBUF6 = 0x1E,
		FRAMEBUF7 = 0x1F,
	};
};

struct Command {
	uint8_t type;
	uint32_t sz;
	uint32_t ptr;
};

#pragma pack(pop)

class Replay {
public:
	Replay(const char *filename);
	~Replay();

	bool Run();

	void SetRange(int start, int end) {
		primStart_ = start;
		primEnd_ = end;
	}

	bool Valid() {
		return valid_;
	}

protected:
	bool ReadCompressed(void *dest, size_t sz, uint32_t version);

	void SyncStall();
	bool SubmitCmds(void *p, u32 sz);
	void SubmitListEnd();

	void Init(u32 ptr, u32 sz);
	void Registers(u32 ptr, u32 sz);
	void Vertices(u32 ptr, u32 sz);
	void Indices(u32 ptr, u32 sz);
	void ClutAddr(u32 ptr, u32 sz);
	void Clut(u32 ptr, u32 sz);
	void TransferSrc(u32 ptr, u32 sz);
	void Memset(u32 ptr, u32 sz);
	void MemcpyDest(u32 ptr, u32 sz);
	void Memcpy(u32 ptr, u32 sz);
	void Texture(int level, u32 ptr, u32 sz);
	void Framebuf(int level, u32 ptr, u32 sz);
	void Display(u32 ptr, u32 sz);
	void EdramTrans(u32 ptr, u32 sz);

	int fd_;
	bool valid_;
	int prims_;
	int primStart_;
	int primEnd_;

	std::vector<Command> cmds_;
	std::vector<uint8_t> buf_;

	void *execMemcpyDest;
	void *execClutAddr;
	u32 execClutFlags;
	u32 *execListBuf;
	u32 *execListPos;
	u32 execListID;
	std::vector<u32> execListQueue;
	u16 lastBufw_[8];
};