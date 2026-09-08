# Kernel-mode counterparts of the sysmem tests. See docs/pspautotests-hardware.md for what
# COMMON_KERNEL does and which libraries a kernel module may import.
TARGETS = partitions heap
EXTRA_OBJS = kernel-imports.o
COMMON_KERNEL = 1

COMMON_DIR = ../../../common
include $(COMMON_DIR)/common.mk
