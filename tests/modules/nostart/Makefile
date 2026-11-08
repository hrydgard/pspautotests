all:
	$(MAKE) --no-print-directory -f child.mak
	$(MAKE) --no-print-directory -f nostart.mak

clean:
	$(MAKE) --no-print-directory -f child.mak clean
	$(MAKE) --no-print-directory -f nostart.mak clean

rebuild: clean all
