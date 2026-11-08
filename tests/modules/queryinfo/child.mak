# No module_start, and the ELF entry is set to 0xFFFFFFFF so the module has no entry at all. Linked
# without pspsdk's crt0_prx (which supplies a module_start) and without libc.

PSPSDK = $(shell psp-config --pspsdk-path)

CFLAGS = -G0 -O0 -Wall -I$(PSPSDK)/include
LDFLAGS = -G0 -nostartfiles -nostdlib -L$(PSPSDK)/lib -Wl,-q,-T$(PSPSDK)/lib/linkfile.prx -Wl,-zmax-page-size=128 -Wl,-e,0xFFFFFFFF

all: child.prx

child_exports.c: child_exports.exp
	psp-build-exports -b $< > $@

child.elf: child.c child_exports.c
	psp-gcc $(CFLAGS) $(LDFLAGS) child.c child_exports.c -lpspmodinfo -o $@

child.prx: child.elf
	psp-prxgen $< $@

clean:
	rm -f child.elf child.prx child_exports.c
