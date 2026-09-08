# A kernel-mode module. Which partitions sceKernelCreateTlspl accepts can depend on the caller's
# privilege, and every other test in the suite runs in user mode.
TARGETS = partition
EXTRA_OBJS = tlspl-imports.o
COMMON_KERNEL = 1

COMMON_DIR = ../../../../common
include $(COMMON_DIR)/common.mk
