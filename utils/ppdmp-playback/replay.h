#pragma once

#include <stdint.h>
#include <stdlib.h>
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

// The dump's payloads. malloc'd, so a dump too big for the PSP is reported instead of written
// through a failed allocation (which took the PSP down).
struct PayloadBuffer {
	uint8_t *p = nullptr;
	size_t n = 0;
	~PayloadBuffer() { free(p); }
	uint8_t *data() { return p; }
	const uint8_t *data() const { return p; }
	size_t size() const { return n; }
};

class Replay {
public:
	Replay(const char *filename);
	~Replay();

	bool Run();

	void SetProgress(int every, int traceFrom) {
		progress_ = every;
		traceFrom_ = traceFrom;
	}
	// Only replay the first count dump commands (0: all), for bisecting a hang.
	void SetCommandLimit(int count) {
		cmdLimit_ = count;
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
	// streamed: the framebuffer snapshots have been trimmed, and the payloads are streamed into place.
	bool LoadPayloads(u32 bufsz, uint32_t version, SceOff dataStart = -1, bool streamed = false);

	// A range of the decompressed payload data to copy somewhere, for StreamPayloads.
	struct PayloadRange {
		u32 ptr;
		u32 size;
		uint8_t *dest;
	};
	// Decompresses the (zstd) payload data starting at dataStart in the file a chunk at a time, copying the ranges out.
	bool StreamPayloads(SceOff dataStart, std::vector<PayloadRange> &ranges);
	// Cuts the framebuffer snapshots the replay won't copy (render targets, unchanged VRAM) down to their headers.
	bool TrimFramebufPayloads(SceOff dataStart, u32 bufsz);

	struct FramebufInfo {
		size_t cmd;
		u32 addr;
		u32 size;
		u32 flags;
	};
	// Sets copyTarget_ for the render target snapshots that hold data a later snapshot leaves out as unchanged.
	void MarkNeededTargets(const std::vector<FramebufInfo> &framebufs);

	void SyncStall();
	void ResetZeroNormalSign();
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
	void CopyAroundDrawn(void *dest, const u8 *src, u32 size, bool linear);

	int fd_;
	bool valid_;
	int prims_;
	int primStart_;
	int primEnd_;
	int progress_ = 0;
	int curCmd_ = 0;
	int traceFrom_ = 0;
	int cmdLimit_ = 0;
	std::vector<u32> alignedRegs_;

	std::vector<Command> cmds_;
	PayloadBuffer buf_;

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
	// Right after INIT, where a CLUT command is the CLUT the GE had loaded.
	bool initialClut_ = false;
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
	// What the replay's draws could reach in one buffer, as merged pixel spans [x1, x2) per row.
	// The same bytes as a list of the drawn rects, but quick to add to and walk with thousands of
	// draws (Megamind 13846 has 180k commands).
	struct DrawnTarget {
		u32 strideBytes;
		u32 bpp;
		std::vector<std::vector<std::pair<int, int>>> rows;
		void Add(const DrawnRect &rect);
	};
	std::map<u32, DrawnTarget> drawnTargets_;
	// By command: the render target snapshots to copy (around what was drawn), see MarkNeededTargets.
	std::vector<bool> copyTarget_;
	u32 region2_ = 0;
	u32 scissor2_ = 0;
	u32 vertType_ = 0;
	u32 lastVertsPtr_ = 0;
	u32 lastVertsSize_ = 0;
};