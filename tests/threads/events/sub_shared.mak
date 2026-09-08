OBJS = $(TARGET).o

BUILD_PRX = 1
PSP_FW_VERSION = 500
PSP_EBOOT_TITLE = events $(TARGET) test

PSP_DRIVE = /cygdrive/j
USE_PSPSDK_LIBC = 1

INCDIR = ../../../../common
CFLAGS = -g -G0 -Wall -O0 -fno-strict-aliasing
# See common/common.mk - these tests pass the kernel deliberately wrong types.
CFLAGS += -Wno-error=int-conversion -Wno-error=incompatible-pointer-types
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR = ../../../../common
LDFLAGS = -G0
LIBS= -lpspgu -lpsprtc -lpspctrl -lpspmath -lpspmpeg -lcommon -lc -lm

EXTRA_TARGETS = EBOOT.PBP
#PSP_EBOOT_ICON = icon.png

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak