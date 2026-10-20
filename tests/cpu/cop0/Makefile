# A kernel-mode module: eret, and the COP0 registers it uses, are privileged.
TARGETS = eret
EXTRA_OBJS = eret-asm.o intrman-imports.o
COMMON_KERNEL = 1

COMMON_DIR = ../../../common
include $(COMMON_DIR)/common.mk
