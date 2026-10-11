all:
	$(MAKE) --no-print-directory -f child.mak
	$(MAKE) --no-print-directory -f loadrace.mak

clean:
	$(MAKE) --no-print-directory -f child.mak clean
	$(MAKE) --no-print-directory -f loadrace.mak clean

rebuild: clean all
