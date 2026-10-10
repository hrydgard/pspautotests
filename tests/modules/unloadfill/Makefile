all:
	$(MAKE) --no-print-directory -f child.mak
	$(MAKE) --no-print-directory -f unloadfill.mak

clean:
	$(MAKE) --no-print-directory -f child.mak clean
	$(MAKE) --no-print-directory -f unloadfill.mak clean

rebuild: clean all
