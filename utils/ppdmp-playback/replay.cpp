#include <pspdisplay.h>
#include <pspge.h>
#include <pspiofilemgr.h>
#include <pspthreadman.h>
#include <psputils.h>
#include <stdio.h>
#include <string.h>
#include "snappy/snappy-c.h"
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
		cmds_.resize(cmdnum);
		buf_.resize(bufsz);
	}

	valid_ = valid_ && ReadCompressed(cmds_.data(), sizeof(Command) * cmdnum, version);
	valid_ = valid_ && ReadCompressed(buf_.data(), bufsz, version);

	sceKernelDcacheWritebackInvalidateRange(buf_.data(), bufsz);

	sceIoClose(fd_);

	primStart_ = 0;
	primEnd_ = 0x7FFFFFFF;
}

bool Replay::ReadCompressed(void *dest, size_t sz, uint32_t version) {
	uint32_t compressed_size = 0;
	if (sceIoRead(fd_, &compressed_size, sizeof(compressed_size)) != sizeof(compressed_size)) {
		return false;
	}

	uint8_t *compressed = new uint8_t[compressed_size];
	if (sceIoRead(fd_, compressed, compressed_size) != (int)compressed_size) {
		delete [] compressed;
		return false;
	}

	size_t real_size = sz;
	if (version < 5)
		snappy_uncompress((const char *)compressed, compressed_size, (char *)dest, &real_size);
	else
		real_size = ZSTD_decompress(dest, real_size, compressed, compressed_size);
	delete [] compressed;

	return real_size == sz;
}

bool Replay::Run() {
	if (!Valid()) {
		return false;
	}

	prims_ = 0;
	for (size_t i = 0; i < cmds_.size(); ++i) {
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
	while (sceGeListSync(execListID, 1) == 2) {
		sceKernelDelayThreadCB(200);
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
		*execListPos++ = (GE_CMD_BASE << 24) | (((uintptr_t)execListBuf >> 8) & 0x00FF0000);
		*execListPos++ = (GE_CMD_JUMP << 24) | ((uintptr_t)execListBuf & 0x00FFFFFF);

		execListPos = execListBuf;
		if (traceFrom_ && curCmd_ >= traceFrom_)
			printf("TRACE: list wrap\n");

		// Don't continue until we've stalled.
		SyncStall();
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
		// TODO: This ignores matrix data and some other things, but it's closer.
		for (int i = 234; i < 512; ++i) {
			ctx->context[i] = GE_CMD_END << 24;
		}
		sceKernelDcacheWritebackInvalidateRange(ctx, sizeof(PspGeContext));
	}

	TrackRegisters((const u32 *)ctx->context + 17, 512 - 17, false);
	sceGeRestoreContext(ctx);
}

void Replay::Registers(u32 ptr, u32 sz) {
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
			sceDmacMemcpy(execClutAddr, buf_.data() + ptr, sz);
			sceKernelDcacheWritebackInvalidateRange(execClutAddr, sz);
		}

		execClutAddr = 0;
	} else {
		uintptr_t psp = (uintptr_t)(buf_.data() + ptr);
		if (psp & 0xF) {
			printf("Clut: uh oh, alignment %d\n", psp & 0xF);
		}
		execListQueue.push_back((GE_CMD_CLUTADDRUPPER << 24) | ((psp >> 8) & 0x00FF0000));
		execListQueue.push_back((GE_CMD_CLUTADDR << 24) | (psp & 0x00FFFFFF));
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
		SyncStall();
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
		SyncStall();
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
	// As GPU/Debugger/Playback.cpp: the GE's own result in a buffer the replay drew to beats the dump's
	// copy, which can be stale.
	const bool isTarget = (framebuf->flags & 1) != 0;
	const bool unchangedVRAM = version_ >= 6 && (framebuf->flags & 2) != 0;
	if (!isTarget && !unchangedVRAM && !DrawnHere((uintptr_t)framebuf->addr)) {
		// After the draws before it.
		SyncStall();
		sceDmacMemcpy(framebuf->addr, psp, pspSize);
		sceKernelDcacheWritebackInvalidateRange(framebuf->addr, pspSize);
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

	sceDisplaySetFrameBuf(disp->topaddr, disp->linesize, disp->pixelFormat, 1);
	sceDisplaySetFrameBuf(disp->topaddr, disp->linesize, disp->pixelFormat, 0);
	haveDisplay_ = true;
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

	DrawnTarget &t = drawnTargets_[start];
	t.strideBytes = fbWidth_ * bpp;
	t.bpp = bpp;
	for (const DrawnRect &r : t.rects) {
		if (r.Contains(x1, y1) && r.Contains(x2, y2))
			return;
	}
	t.rects.push_back(DrawnRect{ x1, y1, x2, y2 });
}

bool Replay::DrawnHere(u32 addr) const {
	const u32 offset = addr & 0x001FFFFF;
	for (auto it = drawnTargets_.begin(); it != drawnTargets_.end() && it->first <= offset; ++it) {
		const DrawnTarget &t = it->second;
		if (t.strideBytes == 0)
			continue;
		const int y = (int)((offset - it->first) / t.strideBytes);
		const int x = (int)((offset - it->first) % t.strideBytes / t.bpp);
		for (const DrawnRect &r : t.rects) {
			if (r.Contains(x, y))
				return true;
		}
	}
	return false;
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
