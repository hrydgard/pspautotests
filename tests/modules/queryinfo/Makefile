all:
	$(MAKE) --no-print-directory -f child.mak
	$(MAKE) --no-print-directory -f queryinfo.mak

clean:
	$(MAKE) --no-print-directory -f child.mak clean
	$(MAKE) --no-print-directory -f queryinfo.mak clean

rebuild: clean all
