all:
	$(MAKE) --no-print-directory -f child.mak
	$(MAKE) --no-print-directory -f startresult.mak

clean:
	$(MAKE) --no-print-directory -f child.mak clean
	$(MAKE) --no-print-directory -f startresult.mak clean

rebuild: clean all
