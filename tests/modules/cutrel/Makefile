all:
	$(MAKE) --no-print-directory -f child.mak
	$(MAKE) --no-print-directory -f cutrel.mak
	$(MAKE) --no-print-directory -C kernel

clean:
	$(MAKE) --no-print-directory -f child.mak clean
	$(MAKE) --no-print-directory -f cutrel.mak clean
	$(MAKE) --no-print-directory -C kernel clean

rebuild: clean all
