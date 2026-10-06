#include <pspdisplay.h>
#include <pspge.h>
#include <pspiofilemgr.h>
#include <pspthreadman.h>
#include <psputils.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <functional>
#include <map>
#include "snappy/snappy.h"
#include "snappy/snappy-sinksource.h"
#define ZSTD_STATIC_LINKING_ONLY
#include "zstd/lib/zstd.h"
#include "commands.h"
#include "replay.h"

extern "C" int sceDmacMemcpy(void *dest, const void *source, unsigned int size);

static const int LIST_BUF_SIZE = 256 * 1024;

Replay::Replay(const char *filename)
	: valid_(true), execMemcpyDest(0), execClutAddr(0), execListBuf(0), execListPos(0), execListID(0) {
	memset(lastBufw_, 0, sizeof(lastBufw_));
	fd_ = sceIoOpen(filename, PSP_O_RDONLY, 0777);
	if (fd_ <= 0) {
		valid_ = false;
		return;
	}

	uint8_t header[8] = { 0 };
	int32_t version = 0;
	uint8_t header2[12] = { 0 };
	valid_ = valid_ && sceIoRead(fd_, header, sizeof(header)) == sizeof(header);
	valid_ = valid_ && sceIoRead(fd_, &version, sizeof(version)) == sizeof(version);
	if (version >= 4) {
		valid_ = valid_ && sceIoRead(fd_, header2, sizeof(header2)) == sizeof(header2);
	}

	static const char *HEADER = "PPSSPPGE";
	static const int MIN_VERSION = 2;
	static const int MAX_VERSION = 6;

	valid_ = valid_ && memcmp(HEADER, header, sizeof(header)) == 0;
	valid_ = valid_ && version >= MIN_VERSION && version <= MAX_VERSION;
	version_ = version;

	uint32_t cmdnum = 0, bufsz = 0;
	valid_ = valid_ && sceIoRead(fd_, &cmdnum, sizeof(cmdnum)) == sizeof(cmdnum);
	valid_ = valid_ && sceIoRead(fd_, &bufsz, sizeof(bufsz)) == sizeof(bufsz);
	if (valid_) {
		// A vector can't report a failed allocation without exceptions, so check there's room first.
		void *probe = malloc(sizeof(Command) * cmdnum + 1);
		if (probe) {
			free(probe);
			cmds_.resize(cmdnum);
		} else {
			printf("ERROR: out of memory for %d commands (%d KB)\n", (int)cmdnum, (int)(sizeof(Command) * cmdnum / 1024));
			valid_ = false;
		}
	}

	valid_ = valid_ && ReadCompressed(cmds_.data(), sizeof(Command) * cmdnum, version);
	valid_ = valid_ && LoadPayloads(bufsz, version);

	if (valid_)
		sceKernelDcacheWritebackInvalidateRange(buf_.data(), buf_.size());

	sceIoClose(fd_);

	primStart_ = 0;
	primEnd_ = 0x7FFFFFFF;
}

static void *AllocOrReport(size_t sz, const char *what) {
	void *p = memalign(16, sz);
	if (!p) {
		// The largest block that is still free, to the nearest 64 KB.
		size_t largest = 0;
		for (size_t step = 64 * 1024 * 1024; step >= 64 * 1024; step /= 2) {
			// volatile, or the compiler drops the unused malloc/free pair and takes every size as free.
			void *volatile q = malloc(largest + step);
			if (q) {
				free(q);
				largest += step;
			}
		}
		printf("ERROR: out of memory: %s needs %d KB, the largest free block is %d KB\n", what, (int)(sz / 1024), (int)(largest / 1024));
	}
	return p;
}

// The dump packs payloads back to back, so one after an odd-sized blob (18 bytes of vertices) starts
// unaligned, and reading a REGISTERS or DISPLAY payload's words there raises an address error on the
// PSP's CPU: the replay hung (Ace Combat ULUS10176, Naruto 16733, Shadow of Destiny 9545). The GE wants
// its vertices aligned too. So every payload gets a 16-byte aligned spot; payloads the dump shares
// between commands stay shared.
//
// To need the memory only once, the data is decompressed into the top of the buffer and each payload
// then moved down to its spot, in source order. base is how high the decompressed data has to sit so
// that no move overwrites a payload that hasn't been moved yet.
bool Replay::LoadPayloads(u32 bufsz, uint32_t version) {
	std::vector<u32> order;
	order.reserve(cmds_.size());
	for (size_t i = 0; i < cmds_.size(); ++i) {
		const u32 ptr = cmds_[i].ptr, sz = cmds_[i].sz;
		if (sz != 0 && (uint64_t)ptr + sz <= bufsz)
			order.push_back((u32)i);
		else
			cmds_[i].ptr = 0;
	}
	// Copies of the fields: Command is packed, and a reference to them would read them with word loads.
	std::sort(order.begin(), order.end(), [this](u32 a, u32 b) {
		const u32 pa = cmds_[a].ptr, pb = cmds_[b].ptr;
		if (pa != pb)
			return pa < pb;
		const u32 sa = cmds_[a].sz, sb = cmds_[b].sz;
		return sa < sb;
	});

	// The alignment the commands using a payload need: the GE reads textures, CLUTs and transfer sources
	// in place at 16 bytes, the rest are read as words.
	auto alignmentOf = [&](size_t i, size_t j) {
		for (size_t k = i; k < j; ++k) {
			const u8 type = cmds_[order[k]].type;
			if ((type >= CommandType::TEXTURE0 && type <= CommandType::TEXTURE7) || type == CommandType::CLUT || type == CommandType::TRANSFERSRC)
				return 16;
		}
		return 4;
	};

	// Payloads that overlap are moved as one block, each pointing into it where its offset keeps its
	// alignment: games draw from many overlapping windows of one vertex buffer, and a copy per window
	// took 40 MB for 10 MB of data (Gundam vs Gundam 13531). One that would be misaligned starts a new
	// block. Walks the distinct payloads in source order, calling payloadFn(first index in order, end
	// index, spot) for each and blockFn(ptr, size, spot) as each block is complete.
	typedef std::function<void(size_t, size_t, size_t)> PayloadFn;
	typedef std::function<void(u32, u32, size_t)> BlockFn;
	auto forEachPayload = [&](const PayloadFn &payloadFn, const BlockFn &blockFn) {
		size_t spot = 0;
		bool open = false;
		u32 blockPtr = 0, blockEnd = 0;
		auto closeBlock = [&]() {
			if (open) {
				blockFn(blockPtr, blockEnd - blockPtr, spot);
				spot += (blockEnd - blockPtr + 15) & ~15;
				open = false;
			}
		};
		size_t i = 0;
		while (i < order.size()) {
			const u32 ptr = cmds_[order[i]].ptr, sz = cmds_[order[i]].sz;
			size_t j = i + 1;
			while (j < order.size() && cmds_[order[j]].ptr == ptr && cmds_[order[j]].sz == sz)
				++j;
			if (open && ptr < blockEnd && ((ptr - blockPtr) % alignmentOf(i, j)) == 0) {
				blockEnd = std::max(blockEnd, ptr + sz);
			} else {
				closeBlock();
				open = true;
				blockPtr = ptr;
				blockEnd = ptr + sz;
			}
			payloadFn(i, j, spot + (ptr - blockPtr));
			i = j;
		}
		closeBlock();
		return spot;
	};

	size_t base = 0;
	size_t prevEnd = 0;
	const size_t total = forEachPayload([](size_t, size_t, size_t) {}, [&](u32 ptr, u32 sz, size_t spot) {
		// The move reads from base + ptr, so the spot can't be above it, and the previous block's move
		// mustn't have reached this block's data (they may overlap).
		if (spot > base + ptr)
			base = spot - ptr;
		if (prevEnd > base + ptr)
			base = prevEnd - ptr;
		prevEnd = spot + sz;
	});

	const size_t alloc = std::max(total, base + (size_t)bufsz);
	buf_.p = (uint8_t *)AllocOrReport(alloc, "the dump's data");
	if (!buf_.p)
		return false;
	if (!ReadCompressed(buf_.p + base, bufsz, version))
		return false;

	forEachPayload([&](size_t i, size_t j, size_t spot) {
		for (size_t k = i; k < j; ++k)
			cmds_[order[k]].ptr = (u32)spot;
	}, [&](u32 ptr, u32 sz, size_t spot) {
		memmove(buf_.p + spot, buf_.p + base + ptr, sz);
	});
	buf_.n = total;
	return true;
}

static const size_t CHUNK_SIZE = 256 * 1024;

// Feeds snappy a compressed block from the file a chunk at a time.
class FileSource : public snappy::Source {
public:
	FileSource(int fd, size_t size, uint8_t *chunk) : fd_(fd), left_(size), chunk_(chunk) {}
	size_t Available() const override {
		return left_ + (len_ - pos_);
	}
	const char *Peek(size_t *len) override {
		if (pos_ == len_ && left_ > 0) {
			const int n = sceIoRead(fd_, chunk_, std::min(left_, CHUNK_SIZE));
			pos_ = 0;
			len_ = n > 0 ? n : 0;
			// A failed read ends the data early, which snappy reports as corrupt.
			left_ = n > 0 ? left_ - n : 0;
		}
		*len = len_ - pos_;
		return (const char *)chunk_ + pos_;
	}
	void Skip(size_t n) override {
		pos_ += n;
	}

private:
	int fd_;
	size_t left_;
	uint8_t *chunk_;
	size_t pos_ = 0;
	size_t len_ = 0;
};

bool Replay::ReadCompressed(void *dest, size_t sz, uint32_t version) {
	uint32_t compressed_size = 0;
	if (sceIoRead(fd_, &compressed_size, sizeof(compressed_size)) != sizeof(compressed_size)) {
		return false;
	}

	// Both formats are streamed in chunks, so the compressed data never has to be in memory whole.
	uint8_t *chunk = (uint8_t *)AllocOrReport(CHUNK_SIZE, "the read buffer");
	if (!chunk)
		return false;

	if (version < 5) {
		FileSource source(fd_, compressed_size, chunk);
		// RawUncompress trusts the length the data starts with, so check it fits first.
		size_t peeked = 0;
		const char *head = source.Peek(&peeked);
		size_t length = 0;
		bool ok = snappy::GetUncompressedLength(head, peeked, &length) && length == sz;
		ok = ok && snappy::RawUncompress(&source, (char *)dest);
		free(chunk);
		return ok;
	}

	// With a stable output buffer zstd decodes straight into dest instead of keeping a window of its own.
	ZSTD_DCtx *dctx = ZSTD_createDCtx();
	bool ok = dctx != nullptr;
	if (ok)
		ok = !ZSTD_isError(ZSTD_DCtx_setParameter(dctx, ZSTD_d_stableOutBuffer, 1));

	ZSTD_outBuffer out = { dest, sz, 0 };
	size_t left = compressed_size;
	while (ok && left > 0) {
		const int n = sceIoRead(fd_, chunk, std::min(left, CHUNK_SIZE));
		if (n <= 0) {
			ok = false;
			break;
		}
		left -= n;
		ZSTD_inBuffer in = { chunk, (size_t)n, 0 };
		while (in.pos < in.size) {
			const size_t r = ZSTD_decompressStream(dctx, &out, &in);
			if (ZSTD_isError(r) || out.pos == out.size) {
				ok = !ZSTD_isError(r);
				break;
			}
		}
	}

	ZSTD_freeDCtx(dctx);
	free(chunk);
	return ok && out.pos == sz;
}

bool Replay::Run() {
	if (!Valid()) {
		return false;
	}

	// As GPU/Debugger/Playback.cpp: the firmware's default EDRAM address translation, which the dump only records
	// when the game changes it. It's GE state, so it would otherwise be whatever the last program left.
	sceGeEdramSetAddrTranslation(0x400);

	prims_ = 0;
	for (size_t i = 0; i < cmds_.size(); ++i) {
		if (cmdLimit_ && (int)i >= cmdLimit_)
			break;
		const Command &cmd = cmds_[i];
		curCmd_ = (int)i;
		if (traceFrom_ && (int)i >= traceFrom_)
			printf("TRACE: %d type %d sz %d\n", (int)i, (int)cmd.type, (int)cmd.sz);
		if (progress_ && (i % progress_) == 0)
			printf("PROGRESS: %d/%d type %d sz %d prims %d\n", (int)i, (int)cmds_.size(), (int)cmd.type, (int)cmd.sz, prims_);
		switch (cmd.type) {
		case CommandType::INIT:
			Init(cmd.ptr, cmd.sz);
			break;

		case CommandType::REGISTERS:
			Registers(cmd.ptr, cmd.sz);
			break;

		case CommandType::VERTICES:
			Vertices(cmd.ptr, cmd.sz);
			break;

		case CommandType::INDICES:
			Indices(cmd.ptr, cmd.sz);
			break;

		case CommandType::CLUTADDR:
			ClutAddr(cmd.ptr, cmd.sz);
			break;

		case CommandType::CLUT:
			Clut(cmd.ptr, cmd.sz);
			break;

		case CommandType::TRANSFERSRC:
			TransferSrc(cmd.ptr, cmd.sz);
			break;

		case CommandType::MEMSET:
			Memset(cmd.ptr, cmd.sz);
			break;

		case CommandType::MEMCPYDEST:
			MemcpyDest(cmd.ptr, cmd.sz);
			break;

		case CommandType::MEMCPYDATA:
			Memcpy(cmd.ptr, cmd.sz);
			break;

		case CommandType::EDRAMTRANS:
			EdramTrans(cmd.ptr, cmd.sz);
			break;

		case CommandType::TEXTURE0:
		case CommandType::TEXTURE1:
		case CommandType::TEXTURE2:
		case CommandType::TEXTURE3:
		case CommandType::TEXTURE4:
		case CommandType::TEXTURE5:
		case CommandType::TEXTURE6:
		case CommandType::TEXTURE7:
			Texture((int)cmd.type - (int)CommandType::TEXTURE0, cmd.ptr, cmd.sz);
			break;

		case CommandType::FRAMEBUF0:
		case CommandType::FRAMEBUF1:
		case CommandType::FRAMEBUF2:
		case CommandType::FRAMEBUF3:
		case CommandType::FRAMEBUF4:
		case CommandType::FRAMEBUF5:
		case CommandType::FRAMEBUF6:
		case CommandType::FRAMEBUF7:
			Framebuf((int)cmd.type - (int)CommandType::FRAMEBUF0, cmd.ptr, cmd.sz);
			break;

		case CommandType::DISPLAY:
			Display(cmd.ptr, cmd.sz);
			break;

		default:
			printf("ERROR: Unsupported GE dump command: %d\n", (int)cmd.type);
			return false;
		}
		if (traceFrom_ && (int)i >= traceFrom_)
			printf("TRACE: done %d\n", (int)i);
	}

	SubmitListEnd();
	return true;
}

void Replay::SyncStall() {
	if (execListBuf == 0) {
		return;
	}

	sceKernelDcacheWritebackInvalidateRange(execListBuf, LIST_BUF_SIZE);
	if (traceFrom_ && curCmd_ >= traceFrom_)
		printf("TRACE: stall update, list state %d\n", sceGeListSync(execListID, 1));
	sceGeListUpdateStallAddr(execListID, execListPos);
	if (traceFrom_ && curCmd_ >= traceFrom_)
		printf("TRACE: stall updated, list state %d\n", sceGeListSync(execListID, 1));

	// We specifically want to wait for 2 to clear, which is why we don't list sync.
	int waited = 0;
	const u32 waitStart = sceKernelGetSystemTimeLow();
	while (sceGeListSync(execListID, 1) == 2) {
		sceKernelDelayThreadCB(200);
		if (traceFrom_ && curCmd_ >= traceFrom_ && sceKernelGetSystemTimeLow() - waitStart > 2000000) {
			// Still DRAWING after 2s: has the GE executed the last commands queued (each register keeps the
			// last value written), or is it stuck before them?
			printf("HUNG: list state %d, list write offset %d\n", sceGeListSync(execListID, 1), (int)(execListPos - execListBuf));
			for (const u32 *w = execListPos - 12; w < execListPos; ++w) {
				if (w >= execListBuf)
					printf("HUNG: queued %08x, GE has %08x\n", *w, sceGeGetCmd(*w >> 24));
			}
			fflush(stdout);
			break;
		}
		if (progress_ && ++waited == 15000) {
			// 3 seconds: report where the GE is, for finding hangs.
			printf("STALL: GE still drawing after 3s, at dump command %d, list write offset %d\n", curCmd_, (int)(execListPos - execListBuf));
		}
	}
}

bool Replay::SubmitCmds(void *p, u32 sz) {
	if (execListBuf == 0) {
		execListBuf = new uint32_t[LIST_BUF_SIZE / 4];
		if (execListBuf == 0) {
			printf("ERROR: Unable to allocate for display list\n");
			return false;
		}
		memset(execListBuf, 0, LIST_BUF_SIZE);
		sceKernelDcacheWritebackInvalidateRange(execListBuf, LIST_BUF_SIZE);

		execListPos = execListBuf;
		*execListPos++ = GE_CMD_NOP << 24;

		execListID = sceGeListEnQueue(execListBuf, execListPos, -1, NULL);
	}

	u32 pendingSize = (int)execListQueue.size() * sizeof(u32);
	// Validate space for jump.
	u32 allocSize = pendingSize + sz + 8;
	if ((uintptr_t)execListPos + allocSize >= (uintptr_t)execListBuf + LIST_BUF_SIZE) {
		// End the list and start a new one at the buffer's start. Jumping back with the stall address moved
		// there let the GE run a lap of old commands (ULJM05302 redrew its background over the cars).
		if (traceFrom_ && curCmd_ >= traceFrom_)
			printf("TRACE: list wrap\n");
		DrainGE();
	}

	memcpy(execListPos, execListQueue.data(), pendingSize);
	execListPos += pendingSize / 4;
	u32 *writePos = execListPos;
	memcpy(execListPos, p, sz);
	execListPos += sz / 4;

	// TODO: Unfortunate.  Maybe Texture commands should contain the bufw instead.
	// The goal here is to realistically combine prims in dumps.  Stalling for the bufw flushes.
	for (u32 i = 0; i < sz / 4; ++i) {
		u32 cmd = writePos[i] >> 24;
		if (cmd >= GE_CMD_TEXBUFWIDTH0 && cmd <= GE_CMD_TEXBUFWIDTH7) {
			int level = cmd - GE_CMD_TEXBUFWIDTH0;
			u16 bufw = writePos[i] & 0xFFFF;

			// NOP the address part of the command to avoid a flush too.
			if (bufw == lastBufw_[level])
				writePos[i] = GE_CMD_NOP << 24;
			else
				writePos[i] = (sceGeGetCmd(GE_CMD_TEXBUFWIDTH0 + level) & 0xFFFF0000) | bufw;
			lastBufw_[level] = bufw;
		}

		if (cmd == GE_CMD_PRIM || cmd == GE_CMD_BEZIER || cmd == GE_CMD_SPLINE || cmd == 0xF7) {
			prims_++;

			// Nuke the command if it's outside the range.
			if (prims_ < primStart_ || prims_ > primEnd_) {
				writePos[i] = GE_CMD_NOP << 24;
			}
		}

		// Since we're here anyway, also NOP out texture addresses.
		// This makes Step Tex not hit phantom textures.
		if (cmd >= GE_CMD_TEXADDR0 && cmd <= GE_CMD_TEXADDR7) {
			writePos[i] = GE_CMD_NOP << 24;
		}
	}

	execListQueue.clear();

	return true;
}

// Finishes everything queued, pixels included, so a CPU or DMA write to VRAM lands after the draws
// before it. Reaching the stall address only means the GE has read the commands: without this, FF Type-0's
// memcpy over its frame lost to the tail of the draw before it.
void Replay::DrainGE() {
	if (execListBuf == 0) {
		return;
	}
	SubmitListEnd();
	sceGeDrawSync(0);

	execListPos = execListBuf;
	*execListPos++ = GE_CMD_NOP << 24;
	sceKernelDcacheWritebackInvalidateRange(execListBuf, LIST_BUF_SIZE);
	execListID = sceGeListEnQueue(execListBuf, execListPos, -1, NULL);
}

void Replay::SubmitListEnd() {
	if (execListPos == 0) {
		return;
	}

	// There's always space for the end, same size as a jump.
	*execListPos++ = GE_CMD_FINISH << 24;
	*execListPos++ = GE_CMD_END << 24;

	SyncStall();
	sceGeListSync(execListID, 0);
}

void Replay::Init(u32 ptr, u32 sz) {
	PspGeContext *ctx = (PspGeContext *)(buf_.data() + ptr);
	bool isOldState = true;
	for (int i = 17; i < 512; ++i) {
		if (ctx->context[i] == GE_CMD_END << 24) {
			isOldState = false;
		}
	}
	if (isOldState) {
		// PPSSPP's old layout (GPU/GPUState.cpp, savedContextVersion 0): 209 register words from index 17
		// (contextCmdRanges), the CLUT load and the five matrix numbers, then the matrices as raw floats.
		// Running the floats as commands hung Shadow of Destiny (ULUS10459), so they become matrix loads,
		// followed by the numbers.
		static const int OLD_MTX_NUMS = 17 + 209 + 1;
		static const struct { u8 numCmd, dataCmd; u8 size; } mtx[5] = {
			{ GE_CMD_BONEMATRIXNUMBER, GE_CMD_BONEMATRIXDATA, 96 },
			{ GE_CMD_WORLDMATRIXNUMBER, GE_CMD_WORLDMATRIXDATA, 12 },
			{ GE_CMD_VIEWMATRIXNUMBER, GE_CMD_VIEWMATRIXDATA, 12 },
			{ GE_CMD_PROJMATRIXNUMBER, GE_CMD_PROJMATRIXDATA, 16 },
			{ GE_CMD_TGENMATRIXNUMBER, GE_CMD_TGENMATRIXDATA, 12 },
		};
		u32 nums[5], floats[148];
		memcpy(nums, &ctx->context[OLD_MTX_NUMS], sizeof(nums));
		memcpy(floats, &ctx->context[OLD_MTX_NUMS + 5], sizeof(floats));
		int out = OLD_MTX_NUMS;
		const u32 *f = floats;
		for (int m = 0; m < 5; ++m) {
			ctx->context[out++] = mtx[m].numCmd << 24;
			for (int i = 0; i < mtx[m].size; ++i)
				ctx->context[out++] = (mtx[m].dataCmd << 24) | (*f++ >> 8);
		}
		for (int m = 0; m < 5; ++m)
			ctx->context[out++] = nums[m];
		while (out < 512)
			ctx->context[out++] = GE_CMD_END << 24;
		sceKernelDcacheWritebackInvalidateRange(ctx, sizeof(PspGeContext));
	}

	// The context's CLUT load reads the game's CLUT address, which here can be anything: Shadow of Destiny's
	// (ULUS10459) points into kernel memory, and loading from it hung the GE. The dump brings the CLUTs it
	// uses as CLUT commands anyway.
	for (int i = 17; i < 512; ++i) {
		if ((ctx->context[i] >> 24) == GE_CMD_END)
			break;
		if ((ctx->context[i] >> 24) == GE_CMD_LOADCLUT)
			ctx->context[i] = GE_CMD_NOP << 24;
	}
	sceKernelDcacheWritebackInvalidateRange(ctx, sizeof(PspGeContext));

	TrackRegisters((const u32 *)ctx->context + 17, 512 - 17, false);
	sceGeRestoreContext(ctx);
	initialClut_ = true;
}

void Replay::Registers(u32 ptr, u32 sz) {
	initialClut_ = false;
	const u8 *data = buf_.data() + ptr;
	// Dumps pack their data without padding, so a register block can follow an odd-sized one, and an
	// unaligned word load crashes the PSP's CPU.
	if ((uintptr_t)data & 3) {
		alignedRegs_.resize(sz / 4);
		memcpy(alignedRegs_.data(), data, sz);
		data = (const u8 *)alignedRegs_.data();
	}
	TrackRegisters((const u32 *)data, sz / 4, true);
	SubmitCmds((void *)data, sz);
}

void Replay::TrackRegisters(const u32 *words, u32 count, bool draws) {
	for (u32 i = 0; i < count; ++i) {
		const u32 op = words[i] >> 24;
		if (op == GE_CMD_REGION2) {
			region2_ = words[i] & 0x000FFFFF;
		} else if (op == GE_CMD_SCISSOR2) {
			scissor2_ = words[i] & 0x000FFFFF;
		} else if (op == GE_CMD_VERTEXTYPE) {
			vertType_ = words[i] & 0x00FFFFFF;
		}
		if (op == GE_CMD_FRAMEBUFPTR) {
			fbPtr_ = words[i] & 0x00FFFFFF;
			haveFramebuf_ = true;
		} else if (op == GE_CMD_FRAMEBUFWIDTH) {
			fbWidth_ = words[i] & 0x07FC;
		} else if (op == GE_CMD_FRAMEBUFPIXFORMAT) {
			fbFormat_ = words[i] & 3;
		} else if (op == GE_CMD_ZBUFPTR) {
			zbPtr_ = words[i] & 0x00FFFFFF;
			haveZbuf_ = true;
		} else if (op == GE_CMD_ZBUFWIDTH) {
			zbWidth_ = words[i] & 0x07FC;
		} else if (op == GE_CMD_ZTESTENABLE) {
			zTest_ = (words[i] & 1) != 0;
		} else if (op == GE_CMD_ZWRITEDISABLE) {
			zWriteDisable_ = (words[i] & 1) != 0;
		} else if (op == GE_CMD_CLEARMODE) {
			clearMode_ = words[i] & 0xFFFF;
		} else if (op == GE_CMD_PRIM || op == GE_CMD_BEZIER || op == GE_CMD_SPLINE) {
			// The depth swizzle follows the color format of the draw that wrote it.
			const bool writesDepth = (clearMode_ & 1) ? (clearMode_ & 0x400) != 0 : (zTest_ && !zWriteDisable_);
			if (writesDepth) {
				depthFormat_ = fbFormat_;
				haveDepthFormat_ = true;
			}
			if (draws)
				MarkDrawn(op == GE_CMD_PRIM ? words[i] & 0x00FFFFFF : 0);
		}
	}
}

void Replay::ShowResult() {
	sceGeDrawSync(0);
	if (haveDisplay_) {
		sceDisplaySetFrameBuf(displayAddr_, displayStride_, displayFormat_, PSP_DISPLAY_SETBUF_NEXTFRAME);
	} else if (haveFramebuf_) {
		sceDisplaySetFrameBuf((void *)(0x04000000 | (fbPtr_ & 0x001FFFF0)), fbWidth_, fbFormat_, PSP_DISPLAY_SETBUF_NEXTFRAME);
	}
	// Two, so the new buffer has been scanned out at least once.
	sceDisplayWaitVblankStart();
	sceDisplayWaitVblankStart();
}

void Replay::Vertices(u32 ptr, u32 sz) {
	lastVertsPtr_ = ptr;
	lastVertsSize_ = sz;
	uintptr_t psp = (uintptr_t)(buf_.data() + ptr);
	if (psp & 0x3) {
		printf("Vertices: uh oh, alignment %d\n", psp & 0x3);
	}

	execListQueue.push_back((GE_CMD_BASE << 24) | ((psp >> 8) & 0x00FF0000));
	execListQueue.push_back((GE_CMD_VADDR << 24) | (psp & 0x00FFFFFF));
}

void Replay::Indices(u32 ptr, u32 sz) {
	uintptr_t psp = (uintptr_t)(buf_.data() + ptr);
	if (psp & 0x3) {
		printf("Indices: uh oh, alignment %d\n", psp & 0x3);
	}
	execListQueue.push_back((GE_CMD_BASE << 24) | ((psp >> 8) & 0x00FF0000));
	execListQueue.push_back((GE_CMD_IADDR << 24) | (psp & 0x00FFFFFF));
}

void Replay::ClutAddr(u32 ptr, u32 sz) {
	const u8 *data = (const u8 *)(buf_.data() + ptr);
	memcpy(&execClutAddr, data, sizeof(execClutAddr));
	memcpy(&execClutFlags, data + 4, sizeof(execClutFlags));
}

void Replay::Clut(u32 ptr, u32 sz) {
	if (execClutAddr != 0) {
		const bool isTarget = (execClutFlags & 1) != 0;

		if (!isTarget) {
			DrainGE();
			sceDmacMemcpy(execClutAddr, buf_.data() + ptr, sz);
			sceKernelDcacheWritebackInvalidateRange(execClutAddr, sz);
		}

		execClutAddr = 0;
	} else {
		uintptr_t psp = (uintptr_t)(buf_.data() + ptr);
		if (psp & 0xF) {
			printf("Clut: uh oh, alignment %d\n", psp & 0xF);
		}
		u32 gameUpper = 0, gameLower = 0;
		if (initialClut_) {
			SyncStall();
			gameUpper = sceGeGetCmd(GE_CMD_CLUTADDRUPPER);
			gameLower = sceGeGetCmd(GE_CMD_CLUTADDR);
		}
		execListQueue.push_back((GE_CMD_CLUTADDRUPPER << 24) | ((psp >> 8) & 0x00FF0000));
		execListQueue.push_back((GE_CMD_CLUTADDR << 24) | (psp & 0x00FFFFFF));
		if (initialClut_) {
			// The CLUT the GE had loaded when the recording started (Record.cpp saves it right after INIT),
			// with no LOADCLUT to follow, unlike the CLUTs recorded at a LOADCLUT: load it here, then put
			// the game's CLUT address back. Without this, draws used whatever CLUT the GE last had
			// (HotBrain 16131, ULUS10268).
			execListQueue.push_back((GE_CMD_LOADCLUT << 24) | ((sz / 32) & 0x3F));
			execListQueue.push_back(gameUpper);
			execListQueue.push_back(gameLower);
			initialClut_ = false;
		}
	}
}

void Replay::TransferSrc(u32 ptr, u32 sz) {
	uintptr_t psp = (uintptr_t)(buf_.data() + ptr);

	// Need to sync in order to access gstate.transfersrcw.
	SyncStall();

	uint32_t transfersrcw = sceGeGetCmd(GE_CMD_TRANSFERSRCW);
	execListQueue.push_back((transfersrcw & 0xFF00FFFF) | ((psp >> 8) & 0x00FF0000));
	execListQueue.push_back(((GE_CMD_TRANSFERSRC) << 24) | (psp & 0x00FFFFFF));
}

static bool IsVRAMAddress(const void *address) {
	return (((uintptr_t)address & 0x3F800000) == 0x04000000);
}

void Replay::Memset(u32 ptr, u32 sz) {
	struct MemsetCommand {
		void *dest;
		int value;
		u32 sz;
	};

	const MemsetCommand *data = (const MemsetCommand *)(buf_.data() + ptr);

	if (IsVRAMAddress(data->dest)) {
		DrainGE();
		memset(data->dest, (uint8_t)data->value, data->sz);
		sceKernelDcacheWritebackInvalidateRange(data->dest, data->sz);
		sceDmacMemcpy((void *)((uintptr_t)data->dest ^ 0x00400000), data->dest, data->sz);
	}
}

void Replay::MemcpyDest(u32 ptr, u32 sz) {
	execMemcpyDest = *(void **)(buf_.data() + ptr);
}

void Replay::Memcpy(u32 ptr, u32 sz) {
	if (IsVRAMAddress(execMemcpyDest)) {
		DrainGE();
		sceDmacMemcpy(execMemcpyDest, buf_.data() + ptr, sz);
		sceKernelDcacheWritebackInvalidateRange(execMemcpyDest, sz);
	}
}

void Replay::Texture(int level, u32 ptr, u32 sz) {
	uintptr_t psp = (uintptr_t)(buf_.data() + ptr);

	if (psp & 0xF) {
		printf("Texture: uh oh, alignment %d\n", psp & 0xF);
	}

	u32 bufwCmd = GE_CMD_TEXBUFWIDTH0 + level;
	u32 addrCmd = GE_CMD_TEXADDR0 + level;
	execListQueue.push_back((bufwCmd << 24) | ((psp >> 8) & 0x00FF0000) | lastBufw_[level]);
	execListQueue.push_back((addrCmd << 24) | (psp & 0x00FFFFFF));
}

void Replay::Framebuf(int level, u32 ptr, u32 sz) {
	struct FramebufData {
		void *addr;
		int bufw;
		u32 flags;
		u32 pad;
	};

	FramebufData *framebuf = (FramebufData *)(buf_.data() + ptr);
	uintptr_t headerSize = (uintptr_t)sizeof(FramebufData);
	uintptr_t pspSize = sz - headerSize;
	const uint8_t *psp = buf_.data() + ptr + headerSize;
	const bool isTarget = (framebuf->flags & 1) != 0;
	const bool unchangedVRAM = version_ >= 6 && (framebuf->flags & 2) != 0;
	if (!isTarget && !unchangedVRAM) {
		CopyAroundDrawn(framebuf->addr, psp, pspSize);
	}

	if ((uintptr_t)framebuf->addr & 0xF) {
		printf("Framebuf: uh oh, alignment %d\n", (uintptr_t)framebuf->addr & 0xF);
	}

	u32 bufwCmd = GE_CMD_TEXBUFWIDTH0 + level;
	u32 addrCmd = GE_CMD_TEXADDR0 + level;
	execListQueue.push_back((bufwCmd << 24) | (((uintptr_t)framebuf->addr >> 8) & 0x00FF0000) | framebuf->bufw);
	execListQueue.push_back((addrCmd << 24) | ((uintptr_t)framebuf->addr & 0x00FFFFFF));
	lastBufw_[level] = framebuf->bufw;
}

void Replay::Display(u32 ptr, u32 sz) {
	struct DisplayBufData {
		uint8_t *topaddr;
		u32 linesize, pixelFormat;
	};

	DisplayBufData *disp = (DisplayBufData *)(buf_.data() + ptr);

	// Sync up drawing.
	SyncStall();
	if (traceFrom_ && curCmd_ >= traceFrom_) {
		printf("TRACE: display synced, list state %d, setting %p %d %d\n", sceGeListSync(execListID, 1), disp->topaddr, (int)disp->linesize, (int)disp->pixelFormat);
		fflush(stdout);
	}

	sceDisplaySetFrameBuf(disp->topaddr, disp->linesize, disp->pixelFormat, 1);
	sceDisplaySetFrameBuf(disp->topaddr, disp->linesize, disp->pixelFormat, 0);
	if (traceFrom_ && curCmd_ >= traceFrom_) {
		printf("TRACE: display set\n");
		fflush(stdout);
	}
	// A display turned off (address 0) shows nothing to compare, so the result falls back to the last
	// framebuffer drawn to, as for a dump without a DISPLAY (Auditorium 9213).
	haveDisplay_ = disp->topaddr != nullptr;
	displayAddr_ = disp->topaddr;
	displayStride_ = disp->linesize;
	displayFormat_ = disp->pixelFormat;
}

bool Replay::SaveDepth(const char *filename) {
	if (!haveZbuf_ || zbWidth_ == 0) {
		return false;
	}
	sceGeDrawSync(0);
	// Through the uncached mirror that undoes the swizzle for the color format the depth was drawn
	// with: 0x600000 for 32-bit color, 0x200000 for 16-bit.
	const u32 format = haveDepthFormat_ ? depthFormat_ : fbFormat_;
	const u32 mirror = format == 3 ? 0x00600000 : 0x00200000;
	const u16 *src = (const u16 *)(0x44000000 + mirror + (zbPtr_ & 0x001FFFF0));
	SceUID fd = sceIoOpen(filename, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if (fd < 0) {
		return false;
	}
	const u32 header[2] = { zbWidth_, 272 };
	sceIoWrite(fd, header, sizeof(header));
	// Through RAM: host0 writes straight from VRAM don't work.
	std::vector<u16> row(zbWidth_);
	for (int y = 0; y < 272; ++y) {
		memcpy(row.data(), src + y * zbWidth_, zbWidth_ * 2);
		sceIoWrite(fd, row.data(), zbWidth_ * 2);
	}
	sceIoClose(fd);
	return true;
}

// The drawing-space bounds of a through mode draw's vertices, or false if they aren't simple to read.
static bool ThroughModeBounds(u32 vtype, const u8 *data, u32 size, u32 count, int &x1, int &y1, int &x2, int &y2) {
	if ((vtype & (1 << 23)) == 0 || (vtype & (3 << 11)) != 0 || (vtype & (7 << 18)) != 0 || count == 0)
		return false;

	static const u8 compSizes[4] = { 0, 1, 2, 4 };
	static const u8 colorSizes[8] = { 0, 0, 0, 0, 2, 2, 2, 4 };
	u32 offset = 0, align = 1;
	auto add = [&](u32 elemSize, u32 n) {
		if (elemSize == 0)
			return;
		offset = (offset + elemSize - 1) & ~(elemSize - 1);
		offset += elemSize * n;
		if (elemSize > align)
			align = elemSize;
	};
	add(compSizes[(vtype >> 9) & 3], ((vtype >> 14) & 7) + 1);
	add(compSizes[vtype & 3], 2);
	add(colorSizes[(vtype >> 2) & 7], 1);
	add(compSizes[(vtype >> 5) & 3], 3);
	const u32 posSize = compSizes[(vtype >> 7) & 3];
	if (posSize == 0)
		return false;
	offset = (offset + posSize - 1) & ~(posSize - 1);
	const u32 posOffset = offset;
	add(posSize, 3);
	const u32 stride = (offset + align - 1) & ~(align - 1);
	if (stride * count > size)
		return false;

	x1 = y1 = 0x7FFFFFFF;
	x2 = y2 = -0x7FFFFFFF;
	for (u32 i = 0; i < count; ++i) {
		const u8 *p = data + i * stride + posOffset;
		int x, y;
		if (posSize == 4) {
			float fx, fy;
			memcpy(&fx, p, 4);
			memcpy(&fy, p + 4, 4);
			if (!(fx == fx) || !(fy == fy) || fx < -4096.0f || fx > 4096.0f || fy < -4096.0f || fy > 4096.0f)
				return false;
			x = (int)fx;
			y = (int)fy;
		} else if (posSize == 2) {
			x = (s16)(p[0] | (p[1] << 8));
			y = (s16)(p[2] | (p[3] << 8));
		} else {
			x = (s8)p[0];
			y = (s8)p[1];
		}
		// Truncation can be a pixel short at either end; the bounds only need to be generous.
		if (x - 1 < x1) x1 = x - 1;
		if (y - 1 < y1) y1 = y - 1;
		if (x + 1 > x2) x2 = x + 1;
		if (y + 1 > y2) y2 = y + 1;
	}
	return true;
}

void Replay::DrawnTarget::Add(const DrawnRect &rect) {
	const int x1 = rect.x1 > 0 ? rect.x1 : 0, x2 = rect.x2 + 1;
	if (x2 <= x1)
		return;
	if ((int)rows.size() <= rect.y2)
		rows.resize(rect.y2 + 1);
	for (int y = rect.y1 > 0 ? rect.y1 : 0; y <= rect.y2; ++y) {
		// Insert [x1, x2) and merge it with the spans it touches, keeping the row sorted.
		std::vector<std::pair<int, int>> &row = rows[y];
		int a = x1, b = x2;
		size_t i = 0;
		while (i < row.size() && row[i].second < a)
			++i;
		size_t j = i;
		while (j < row.size() && row[j].first <= b) {
			a = std::min(a, row[j].first);
			b = std::max(b, row[j].second);
			++j;
		}
		row.erase(row.begin() + i, row.begin() + j);
		row.insert(row.begin() + i, std::make_pair(a, b));
	}
}

void Replay::MarkDrawn(u32 prim) {
	const u32 start = fbPtr_ & 0x001FFFF0;
	const u32 bpp = fbFormat_ == 3 ? 4 : 2;
	int x1 = 0, y1 = 0;
	int x2 = (region2_ & 0x3FF) < (scissor2_ & 0x3FF) ? (region2_ & 0x3FF) : (scissor2_ & 0x3FF);
	int y2 = ((region2_ >> 10) & 0x3FF) < ((scissor2_ >> 10) & 0x3FF) ? ((region2_ >> 10) & 0x3FF) : ((scissor2_ >> 10) & 0x3FF);
	int vx1, vy1, vx2, vy2;
	if (prim != 0 && lastVertsSize_ != 0 && ThroughModeBounds(vertType_, buf_.data() + lastVertsPtr_, lastVertsSize_, prim & 0xFFFF, vx1, vy1, vx2, vy2)) {
		if (vx1 > x1) x1 = vx1;
		if (vy1 > y1) y1 = vy1;
		if (vx2 < x2) x2 = vx2;
		if (vy2 < y2) y2 = vy2;
		if (x1 > x2 || y1 > y2)
			return;
	}

	auto add = [&](u32 base, u32 strideBytes, u32 bytesPerPixel, const DrawnRect &rect) {
		DrawnTarget &t = drawnTargets_[base];
		t.strideBytes = strideBytes;
		t.bpp = bytesPerPixel;
		t.Add(rect);
	};
	add(start, fbWidth_ * bpp, bpp, DrawnRect{ x1, y1, x2, y2 });
	const bool writesDepth = (clearMode_ & 1) ? (clearMode_ & 0x400) != 0 : (zTest_ && !zWriteDisable_);
	if (writesDepth && zbWidth_ != 0) {
		// Depth is stored swizzled, so a drawn area spreads over whole rows in bands.
		add(zbPtr_ & 0x001FFFF0, zbWidth_ * 2, 2, DrawnRect{ 0, y1 & ~7, (int)zbWidth_ - 1, y2 | 7 });
	}
}

// As GPU/Debugger/Playback.cpp: the dump's copy of a buffer can be stale, and the GE's own result in what
// the replay drew beats it, so the copy leaves out the areas the replay's draws could reach. Waits for the
// GE before writing anything.
void Replay::CopyAroundDrawn(void *dest, const u8 *src, u32 size) {
	// A texture recorded at its full size can run past the end of VRAM, into the depth swizzle mirror.
	if (IsVRAMAddress(dest) && size > 0x00200000 - ((uintptr_t)dest & 0x001FFFFF))
		size = 0x00200000 - ((uintptr_t)dest & 0x001FFFFF);
	std::vector<std::pair<int64_t, int64_t>> skip;
	const int64_t start = (uintptr_t)dest & 0x001FFFFF, end = start + size;
	for (auto it = drawnTargets_.begin(); it != drawnTargets_.end(); ++it) {
		const DrawnTarget &t = it->second;
		if (t.strideBytes == 0)
			continue;
		// Only the rows that can reach [start, end). A span ends by x 1024 (the region's limit), which can
		// be past the stride.
		const int64_t first = (start - (int64_t)it->first - 1024 * (int64_t)t.bpp) / (int64_t)t.strideBytes - 1;
		const int64_t last = (end - (int64_t)it->first) / (int64_t)t.strideBytes + 1;
		for (int64_t y = first > 0 ? first : 0; y <= last && y < (int64_t)t.rows.size(); ++y) {
			const int64_t row = (int64_t)it->first + y * t.strideBytes;
			for (const std::pair<int, int> &span : t.rows[y]) {
				const int64_t a = row + (int64_t)span.first * t.bpp;
				const int64_t b = row + (int64_t)span.second * t.bpp;
				if (b > start && a < end)
					skip.push_back(std::make_pair(a > start ? a : start, b < end ? b : end));
			}
		}
	}
	std::sort(skip.begin(), skip.end());

	int64_t pos = start;
	bool drained = false;
	auto copyTo = [&](int64_t until) {
		if (until > pos) {
			// The GE has to finish with the old contents first, but a copy that falls entirely inside
			// what the replay drew writes nothing and needn't wait (thousands of them in Megamind 13846).
			if (!drained) {
				DrainGE();
				drained = true;
			}
			// Through the uncached mirror: the pieces can be any multiple of 2 bytes.
			u8 *d = (u8 *)(((uintptr_t)dest | 0x40000000) + (pos - start));
			memcpy(d, src + (pos - start), (u32)(until - pos));
		}
	};
	for (size_t i = 0; i < skip.size(); ++i) {
		copyTo(skip[i].first);
		if (skip[i].second > pos)
			pos = skip[i].second;
	}
	copyTo(end);
}

void Replay::EdramTrans(u32 ptr, u32 sz) {
	uint32_t value;
	memcpy(&value, buf_.data() + ptr, 4);

	SyncStall();

	sceGeEdramSetAddrTranslation(value);
}

Replay::~Replay() {
	delete [] execListBuf;
}
