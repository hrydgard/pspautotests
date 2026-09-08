# Include this to simplify your test include file.
#
# Usage:
#
# TARGETS=mytest
# include ../../path/to/common/common.mk

BUILD_PRX = 1
USE_PSPSDK_LIBC = 1
PSP_FW_VERSION = 500

ifdef COMMON_KERNEL
# The stock crt0_prx references __libcglue_init, which drags in all of libcglue - and libcglue
# imports sceNetInet, sceUtility and the ForUser IO libraries. A kernel module that imports those
# won't load (8002013C, library not found), and no amount of trimming LIBS avoids it because the
# reference comes from the startup object itself. USE_KERNEL_LIBS picks the kernel startup
# instead; common.c supplies the handful of newlib support functions that go missing with it.
USE_KERNEL_LIBS = 1
endif

INCDIR := $(INCDIR) . $(COMMON_DIR)
LIBDIR := $(LIBDIR) . $(COMMON_DIR)

ifndef CFLAGS
CFLAGS = -g -G0 -Wall -O0 -fno-strict-aliasing
endif
# Tests deliberately hand the kernel the wrong type - a NULL or 0xDEADBEEF where a SceUID goes,
# an int stuffed into a void * callback argument - to see what it does with it. GCC 14 promoted
# both of these from warnings to errors; put them back to warnings rather than casting away the
# very thing being tested.
CFLAGS := $(CFLAGS) -Wno-error=int-conversion -Wno-error=incompatible-pointer-types
ifndef CXXFLAGS
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
endif
# Tests deliberately feed out-of-range constants (error codes, 0x8000xxxx values, 0xAB as a char)
# through braced initializers to see what the kernel does with them. C++11 turned that from a
# warning into an error, so keep it a warning.
CXXFLAGS := $(CXXFLAGS) -Wno-narrowing
ifndef ASFLAGS
ASFLAGS = $(CFLAGS)
endif
ifndef LDFLAGS
LDFLAGS = -G0
endif

ifndef LIBS
ifdef COMMON_KERNEL
LIBS = -lcommon_kernel -lc -lm
else
LIBS = -lpspgu -lpsprtc -lpspctrl -lpspmath -lcommon -lc -lm
endif
endif
ifdef EXTRA_LIBS
LIBS := $(LIBS) $(EXTRA_LIBS)
endif
# The .elf rule below links everything with the C driver, so C++ tests don't otherwise get
# operator new/delete.
LIBS := $(LIBS) -lstdc++

TARGET = $(firstword $(TARGETS))
OBJS = $(firstword $(TARGETS)).o $(EXTRA_OBJS)

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

ifdef COMMON_KERNEL
# build.mak has composed the kernel library list by now; add ours and newlib in front of it.
# This has to come after the include, since that's where the composing happens.
LIBS := -lcommon_kernel $(LIBS) -lc -lm -lgcc
endif

%.elf: %.o $(EXTRA_OBJS) $(EXPORT_OBJ)
	$(LINK.c) $^ $(LIBS) -o $@
	$(FIXUP) $@

%.prx: %.elf
	psp-prxgen $< $@

%.o: %.S
	$(AS) $(ASFLAGS) -c -o $@ $<

all: $(TARGETS:=.prx)
clean: EXTRA_TARGETS:=$(EXTRA_TARGETS) $(TARGETS:=.prx)
