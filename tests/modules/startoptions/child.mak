# The child defines its own module_start/module_stop, so it's linked without pspsdk's crt0_prx
# (which supplies a module_start that calls main) and without libc.

PSPSDK = $(shell psp-config --pspsdk-path)

CFLAGS = -G0 -O0 -Wall -I$(PSPSDK)/include
LDFLAGS = -G0 -nostartfiles -nostdlib -L$(PSPSDK)/lib -Wl,-q,-T$(PSPSDK)/lib/linkfile.prx -Wl,-zmax-page-size=128

all: child.prx

child_exports.c: child_exports.exp
	psp-build-exports -b $< > $@

child.elf: child.c child_exports.c shared.h
	psp-gcc $(CFLAGS) $(LDFLAGS) child.c child_exports.c -lpspmodinfo -lpspuser -o $@
	psp-fixup-imports $@

child.prx: child.elf
	psp-prxgen $< $@

clean:
	rm -f child.elf child.prx child_exports.c
