#pragma once

#include <stdint.h>
#include <map>
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

	void SetProgress(int every, int traceFrom) {
		progress_ = every;
		traceFrom_ = traceFrom;
	}
	void SetRange(int start, int end) {
		primStart_ = start;
		primEnd_ = end;
	}

	bool Valid() {
		return valid_;
	}

	// Waits for rendering to finish and puts the result on the display: the dump's last DISPLAY,
	// or the last framebuffer it drew to when it has none.
	void ShowResult();

	// Writes the depth buffer the frame ended with, deswizzled, as u32 width, u32 height, then u16s.
	bool SaveDepth(const char *filename);

protected:
	bool ReadCompressed(void *dest, size_t sz, uint32_t version);

	void SyncStall();
	bool SubmitCmds(void *p, u32 sz);
	void SubmitListEnd();
	void DrainGE();

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
	void TrackRegisters(const u32 *words, u32 count, bool draws);
	void MarkDrawn(u32 prim);
	void CopyAroundDrawn(void *dest, const u8 *src, u32 size);

	int fd_;
	bool valid_;
	int prims_;
	int primStart_;
	int primEnd_;
	int progress_ = 0;
	int curCmd_ = 0;
	int traceFrom_ = 0;
	std::vector<u32> alignedRegs_;

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

	bool haveDisplay_ = false;
	void *displayAddr_ = nullptr;
	u32 displayStride_ = 0;
	u32 displayFormat_ = 0;
	bool haveFramebuf_ = false;
	u32 fbPtr_ = 0;
	u32 fbWidth_ = 0;
	u32 fbFormat_ = 0;
	bool haveZbuf_ = false;
	u32 zbPtr_ = 0;
	u32 zbWidth_ = 0;
	bool zTest_ = false;
	bool zWriteDisable_ = false;
	u32 clearMode_ = 0;
	u32 depthFormat_ = 0;
	bool haveDepthFormat_ = false;
	int32_t version_ = 0;

	// As GPU/Debugger/Playback.cpp: the framebuffers drawn to so far by VRAM offset, with the areas the
	// draws could reach.
	struct DrawnRect {
		int x1, y1, x2, y2;
		bool Contains(int x, int y) const {
			return x >= x1 && x <= x2 && y >= y1 && y <= y2;
		}
	};
	struct DrawnTarget {
		u32 strideBytes;
		u32 bpp;
		std::vector<DrawnRect> rects;
	};
	std::map<u32, DrawnTarget> drawnTargets_;
	u32 region2_ = 0;
	u32 scissor2_ = 0;
	u32 vertType_ = 0;
	u32 lastVertsPtr_ = 0;
	u32 lastVertsSize_ = 0;
};