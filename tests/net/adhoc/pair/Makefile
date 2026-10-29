TARGETS = basic_host basic_client matching_host matching_client vanish_host vanish_client matching2_host matching2_client gamemode_host gamemode_client lifecycle_host lifecycle_client flow_host flow_client matching3_host matching3_client timing_host timing_client ptpclose_host ptpclose_client matchingjoin_host matchingjoin_client vanishtiming_host vanishtiming_client ptplockstep_host ptplockstep_client gamemodescan_host gamemodescan_client ptpconnect_host ptpconnect_client gamemodetiming_host gamemodetiming_client
EXTRA_OBJS = ../../net-imports.o

COMMON_DIR = ../../../../common
include $(COMMON_DIR)/common.mk

# Each test is built from X.inc through X_host.c and X_client.c, and they all include these headers.
# make doesn't know about any of that by itself.
$(addsuffix .o,$(TARGETS)): pair.h pairmatching.h ../adhoc.h
$(foreach t,$(sort $(subst _host,,$(subst _client,,$(TARGETS)))),$(eval $(t)_host.o $(t)_client.o: $(t).inc))
timing_host.o timing_client.o: ../timing.h
