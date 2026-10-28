TARGETS = blocking bind poll errors options addr udp tcp stats offline msg polledge states reuse limits
EXTRA_OBJS = ../net-imports.o

COMMON_DIR = ../../../common
include $(COMMON_DIR)/common.mk

# The new tests include inet.h, and make doesn't know that by itself.
errors.o options.o addr.o udp.o tcp.o stats.o offline.o msg.o polledge.o states.o reuse.o limits.o: inet.h
