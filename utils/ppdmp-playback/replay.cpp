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
	}

	SubmitListEnd();
	return true;
}

void Replay::SyncStall() {
	if (execListBuf == 0) {
		return;
	}

	sceKernelDcacheWritebackInvalidateRange(execListBuf, LIST_BUF_SIZE);
	sceGeListUpdateStallAddr(execListID, execListPos);

	// We specifically want to wait for 2 to clear, which is why we don't list sync.
	while (sceGeListSync(execListID, 1) == 2) {
		sceKernelDelayThreadCB(200);
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

	sceGeRestoreContext(ctx);
}

void Replay::Registers(u32 ptr, u32 sz) {
	SubmitCmds(buf_.data() + ptr, sz);
}

void Replay::Vertices(u32 ptr, u32 sz) {
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
	if ((framebuf->flags & 1) == 0) {
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
