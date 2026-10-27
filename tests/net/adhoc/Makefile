TARGETS = pdp adhocctl ptp matching self poll gamemode apctl loopback discover
EXTRA_OBJS = adhoc-imports.o

COMMON_DIR = ../../../common
include $(COMMON_DIR)/common.mk

# The tests' sources include these, and make doesn't know that by itself.
$(addsuffix .o,$(TARGETS)): adhoc.h
