# child_cut.prx is child.prx cut off where its first relocation section starts. psp-prxgen puts the
# section header table before the relocations, so the headers survive and only the relocation and
# string data are missing - the shape of PMDVIS's kcall.prx.

PSPSDK = $(shell psp-config --pspsdk-path)

CFLAGS = -G0 -O0 -Wall -I$(PSPSDK)/include
LDFLAGS = -G0 -nostartfiles -nostdlib -L$(PSPSDK)/lib -Wl,-q,-T$(PSPSDK)/lib/linkfile.prx -Wl,-zmax-page-size=128

all: child.prx child_cut.prx child_k.prx child_k_cut.prx child_n.prx child_n_cut.prx

child_exports.c: child_exports.exp
	psp-build-exports -b $< > $@

child.elf: child.c child_exports.c
	psp-gcc $(CFLAGS) $(LDFLAGS) child.c child_exports.c -lpspmodinfo -lpspuser -o $@

child.prx: child.elf
	psp-prxgen $< $@

# No export library besides syslib.
child_n.elf: child.c
	psp-gcc $(CFLAGS) $(LDFLAGS) child.c -lpspmodinfo -lpspuser -o $@

child_n.prx: child_n.elf
	psp-prxgen $< $@

child_k.elf: child.c child_exports.c
	psp-gcc $(CFLAGS) -DKERNEL $(LDFLAGS) child.c child_exports.c -lpspmodinfo -lpspuser -o $@

child_k.prx: child_k.elf
	psp-prxgen $< $@

%_cut.prx: %.prx
	head -c $$((0x$$(psp-readelf -S $< | awk '/LOPROC\+0xa0/ { for (i = 1; i <= NF; i++) if ($$i ~ /LOPROC/) { print $$(i + 2); exit } }'))) $< > $@

clean:
	rm -f child.elf child.prx child_cut.prx child_k.elf child_k.prx child_k_cut.prx child_n.elf child_n.prx child_n_cut.prx child_exports.c
