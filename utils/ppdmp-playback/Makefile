BUILD_PRX = 1
USE_PSPSDK_LIBC = 1
PSP_FW_VERSION = 500

COMMON_DIR = ../../common
INCDIR := $(INCDIR) . $(COMMON_DIR) $(COMMON_DIR)/ext/
LIBDIR := $(LIBDIR) . $(COMMON_DIR) $(COMMON_DIR)/ext/

CFLAGS = -G0 -Wall -O2 -fno-strict-aliasing
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti -std=gnu++0x
ASFLAGS = $(CFLAGS)
LDFLAGS =

LIBS = -lcommon -lpspgu -lpsppower -lstdc++ -lc -lm
TARGET = playback

OBJS = playback.o replay.o \
	dmac-imports.o $(COMMON_DIR)/ext/snappy/snappy.o $(COMMON_DIR)/ext/snappy/snappy-c.o \
	$(COMMON_DIR)/ext/zstd/lib/common/debug.o $(COMMON_DIR)/ext/zstd/lib/common/entropy_common.o $(COMMON_DIR)/ext/zstd/lib/common/error_private.o \
	$(COMMON_DIR)/ext/zstd/lib/common/fse_decompress.o $(COMMON_DIR)/ext/zstd/lib/common/pool.o $(COMMON_DIR)/ext/zstd/lib/common/threading.o \
	$(COMMON_DIR)/ext/zstd/lib/common/xxhash.o $(COMMON_DIR)/ext/zstd/lib/common/zstd_common.o \
	$(COMMON_DIR)/ext/zstd/lib/decompress/huf_decompress.o $(COMMON_DIR)/ext/zstd/lib/decompress/zstd_ddict.o \
	$(COMMON_DIR)/ext/zstd/lib/decompress/zstd_decompress.o $(COMMON_DIR)/ext/zstd/lib/decompress/zstd_decompress_block.o

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

%.o: %.S
	$(AS) $(ASFLAGS) -c -o $@ $<
