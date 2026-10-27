TARGETS = lan_host lan_client
EXTRA_OBJS = ../net-imports.o pair-imports.o

COMMON_DIR = ../../../../common
include $(COMMON_DIR)/common.mk

# Each test is built from X.inc through X_host.c and X_client.c, and make doesn't know that by itself.
$(addsuffix .o,$(TARGETS)): ../inet.h
$(foreach t,$(sort $(subst _host,,$(subst _client,,$(TARGETS)))),$(eval $(t)_host.o $(t)_client.o: $(t).inc))
