all:
	$(MAKE) --no-print-directory -f child.mak
	$(MAKE) --no-print-directory -f selfunload.mak

clean:
	$(MAKE) --no-print-directory -f child.mak clean
	$(MAKE) --no-print-directory -f selfunload.mak clean

rebuild: clean all
