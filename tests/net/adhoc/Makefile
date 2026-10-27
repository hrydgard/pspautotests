TARGETS = pdp adhocctl ptp matching self poll gamemode apctl loopback discover limits alerts timing
EXTRA_OBJS = ../net-imports.o

COMMON_DIR = ../../../common
include $(COMMON_DIR)/common.mk

# The tests' sources include these, and make doesn't know that by itself.
$(addsuffix .o,$(TARGETS)): adhoc.h

timing.o: timing.h adhoc.h
